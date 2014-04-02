/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

#include <cstdlib>
#include <sstream>

#include <be_error.h>
#include <be_io_sqliterecstore.h>
#include <be_io_utility.h>

/* 
 * RHEL5 sqlite distributions are lacking most of the modern SQLite3 API.
 * SQLITE_OPEN_READONLY is a symbol #define'd in versions that support
 * the complete API.
 */
#ifdef	SQLITE_OPEN_READONLY
#define	SQLITE_V2_SUPPORT	1
#endif

const std::string
     BiometricEvaluation::IO::SQLiteRecordStore::KEY_COL = "key";
const std::string
     BiometricEvaluation::IO::SQLiteRecordStore::VALUE_COL = "value";
const std::string
     BiometricEvaluation::IO::SQLiteRecordStore::PRIMARY_KV_TABLE =
     "RecordStore";
const std::string
    BiometricEvaluation::IO::SQLiteRecordStore::SUBORDINATE_KV_TABLE =
    "SubordinateRecordStore";

/* 
 * The maximum record size supported by the underlying SQLite file is
 * 2^30 by default, and never larger than 2^31. This class will break
 * larger records up into multiple key/value pairs, creating the new 
 * keys using a reserved key character.
 *
 * NOTE:
 * The maximum size of a record can change when SQLite is compiled, so
 * the value below is set at the default.  V2 allows one to lookup the maximum,
 * but this will be different system to system.  Examine the maximum for your
 * system if SQLiteRecordStore appears to be truncating data read from
 * large records created on another system.
 */
static const uint64_t MAX_REC_SIZE = (uint64_t)1000000000U;


BiometricEvaluation::IO::SQLiteRecordStore::SQLiteRecordStore(
    const std::string &name,
    const std::string &description,
    const std::string &parentDir) :
    RecordStore(name,
    description,
    RecordStore::Kind::SQLite,
    parentDir),
    _db(nullptr),
    _dbname(""),
    _sequencer(nullptr),
    _sequenceEnd(false)
{
#ifdef	SQLITE_V2_SUPPORT
	sqlite3_initialize();
#endif

	_dbname = getDirectory() + '/' + getName();
	if (IO::Utility::fileExists(_dbname))
		throw Error::ObjectExists("Database already exists");
		
	/* TODO: SQLITE_OPEN_PRIVATECACHE */
#ifdef	SQLITE_V2_SUPPORT
	int32_t rv = sqlite3_open_v2(_dbname.c_str(), &_db,
	    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
	    SQLITE_OPEN_FULLMUTEX, nullptr);
#else
	int32_t rv = sqlite3_open(_dbname.c_str(), &_db);
#endif
	if ((rv != SQLITE_OK) || (_db == nullptr))
		sqliteError(rv);
	
	this->createStructure();
	_cursorRow = 0;
}

BiometricEvaluation::IO::SQLiteRecordStore::SQLiteRecordStore(
    const std::string &name,
    const std::string &parentDir,
    uint8_t mode) :
    RecordStore(name,
    parentDir,
    mode),
    _db(nullptr),
    _dbname(""),
    _sequencer(nullptr),
    _sequenceEnd(false)
{
#ifdef	SQLITE_V2_SUPPORT
	sqlite3_initialize();
#endif

	_dbname = getDirectory() + '/' + getName();
	if (!IO::Utility::fileExists(_dbname))
		throw Error::ObjectDoesNotExist("Database does not exist");
		
	int32_t rv;
#ifdef	SQLITE_V2_SUPPORT
	if (mode == READWRITE)
		/* TODO: SQLITE_OPEN_PRIVATECACHE */
		rv = sqlite3_open_v2(_dbname.c_str(), &_db, 
		    SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr);
	else
		/* TODO: SQLITE_OPEN_PRIVATECACHE */
		rv = sqlite3_open_v2(_dbname.c_str(), &_db, 
		    SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr);
#else
	rv = sqlite3_open(_dbname.c_str(), &_db);
#endif
	if ((rv != SQLITE_OK) || (_db == nullptr))
		sqliteError(rv);
	
	if (this->validateSchema() == false)
		throw Error::StrategyError("sqlite3: Invalid schema");
		
	_cursorRow = 0;
}

BiometricEvaluation::IO::SQLiteRecordStore::~SQLiteRecordStore()
{
	this->cleanup();
		
	/* NOT THREAD SAFE! */
//	sqlite3_shutdown();
}

void
BiometricEvaluation::IO::SQLiteRecordStore::changeName(
    const std::string &name)
{
	if (getMode() == IO::READONLY)
		throw Error::StrategyError("RecordStore was opened read-only");

	this->cleanup();
		
	std::string oldDBName, newDBName;
	if (getParentDirectory().empty() || getParentDirectory() == ".") {
		oldDBName = name + '/' + getName();
		newDBName = name + '/' + name;
	} else {
		oldDBName = getParentDirectory() + '/' + name + '/' + getName();
		newDBName = getParentDirectory() + '/' + name + '/' + name;
	}
	RecordStore::changeName(name);
	if (rename(oldDBName.c_str(), newDBName.c_str()))
		throw Error::StrategyError("sqlite3: Could not rename "
		    "database (" + Error::errorStr() + ")");
		    
	_dbname = RecordStore::canonicalName(getName());
	if (!IO::Utility::fileExists(_dbname))
		throw Error::StrategyError("sqlite3: Database " + _dbname + 
		    "does not exist");

#ifdef	SQLITE_V2_SUPPORT
	int32_t rv = sqlite3_open_v2(_dbname.c_str(), &_db, 
	    SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr);
#else
	int32_t rv = sqlite3_open(_dbname.c_str(), &_db);
#endif
    	if ((rv != SQLITE_OK) || (_db == nullptr))
		sqliteError(rv);
	
	if (this->validateSchema() == false)
		throw Error::StrategyError("sqlite3: Invalid schema");
}

void
BiometricEvaluation::IO::SQLiteRecordStore::changeDescription(
    const std::string &description)
{
	RecordStore::changeDescription(description);
}
			
uint64_t
BiometricEvaluation::IO::SQLiteRecordStore::getSpaceUsed()
    const
{
	this->sync();
	return (RecordStore::getSpaceUsed() + 
	    IO::Utility::getFileSize(_dbname));
}

void
BiometricEvaluation::IO::SQLiteRecordStore::insert(
    const std::string &key,
    const void *const data,
    const uint64_t size)
{
	if (getMode() == IO::READONLY)
		throw Error::StrategyError("RecordStore was opened read-only");
	if (!validateKeyString(key))
		throw Error::StrategyError("Invalid key format");
	
	/* Warn if key is already in database */
	try {
		this->length(key);
		throw Error::ObjectExists(key);
	} catch (Error::ObjectDoesNotExist) {}
	
	sqlite3_stmt *statement = nullptr;
	std::string activeTable = PRIMARY_KV_TABLE;
	uint64_t segnum = 0;
	uint64_t remSize = size, bindSize = 0;
	uint8_t *bindData = (uint8_t *)data;
	while ((remSize > 0) ||
	    ((remSize == 0) && (segnum < KEY_SEGMENT_START))) {
		std::string sqlCommand = "INSERT INTO " + activeTable + " " + 
		    "VALUES ('" + genKeySegName(key, segnum) +
		    "', $value)";
	
		/* Prepare the statement */
#ifdef	SQLITE_V2_SUPPORT
		int32_t rv = sqlite3_prepare_v2(_db, sqlCommand.c_str(),
		    sqlCommand.length(), &statement, nullptr);
#else
		int32_t rv = sqlite3_prepare(_db, sqlCommand.c_str(),
		    sqlCommand.length(), &statement, nullptr);
#endif
		if (rv != SQLITE_OK) {
			sqlite3_finalize(statement);
			sqliteError(rv);
		}
		if (statement == nullptr)
			throw Error::StrategyError("SQLite: Could not allocate "
			    "statement");
	
		/* Bind data to the statement, segmenting if necessary */
		if (remSize < MAX_REC_SIZE) {
			bindSize = remSize;
			remSize = 0;
		} else {
			bindSize = MAX_REC_SIZE;
			remSize -= MAX_REC_SIZE;
		}
		rv = sqlite3_bind_blob(statement,
		    sqlite3_bind_parameter_index(statement, "$value"),
		    bindData, bindSize, SQLITE_STATIC);
		if (rv != SQLITE_OK) {
			sqlite3_finalize(statement);
			sqliteError(rv);
		}
			
		/* Execute the statement */
		rv = sqlite3_step(statement);
		if (rv != SQLITE_DONE) {
			sqlite3_finalize(statement);
			sqliteError(rv);
		}
			
		/* Free the statement */
		rv = sqlite3_finalize(statement);
		if (rv != SQLITE_OK)
			sqliteError(rv);
			
		/* Increment data position and segment */
		bindData += bindSize;
		switch (segnum) {
		case 0:
			segnum = KEY_SEGMENT_START;
			activeTable = SUBORDINATE_KV_TABLE;
			break;
		default:
			segnum++;
			break;
		}
	}
	
	/* Propagate to parent class */
	RecordStore::insert(key, data, size);
}

void
BiometricEvaluation::IO::SQLiteRecordStore::remove(
    const std::string &key)
{
	if (getMode() == IO::READONLY)
		throw Error::StrategyError("RecordStore was opened read-only");
	if (!validateKeyString(key))
		throw Error::StrategyError("Invalid key format");

	sqlite3_stmt *statement;
	std::string activeTable = PRIMARY_KV_TABLE;
	int64_t segnum = 0;
	bool moreSegments = true;
	while (moreSegments) {
		std::string sqlCommand = "DELETE FROM " + activeTable + " " + 
		    "WHERE " + KEY_COL + " = '" + genKeySegName(key, segnum) + 
		    "'";
		
		/* Prepare the statement */
#ifdef	SQLITE_V2_SUPPORT
		int32_t rv = sqlite3_prepare_v2(_db, sqlCommand.c_str(),
		    sqlCommand.length(), &statement, nullptr);
#else
		int32_t rv = sqlite3_prepare(_db, sqlCommand.c_str(),
		    sqlCommand.length(), &statement, nullptr);
#endif
		if (rv != SQLITE_OK) {
			sqlite3_finalize(statement);
			sqliteError(rv);
		}
		if (statement == nullptr)
			throw Error::StrategyError("SQLite: Could not allocate "
			    "statement");
	
		/* Execute the statement */
		rv = sqlite3_step(statement);
		if (rv != SQLITE_DONE) {
			sqlite3_finalize(statement);
			sqliteError(rv);
		}
		
		/* Free the statement */
		rv = sqlite3_finalize(statement);
		if (rv != SQLITE_OK)
			sqliteError(rv);
		
		/* Increment segment number */
		switch (segnum) {
		case 0:
			/* Check if any rows were actually deleted */
			if (sqlite3_changes(_db) == 0)
				throw Error::ObjectDoesNotExist(key);
				
			segnum = KEY_SEGMENT_START;
			activeTable = SUBORDINATE_KV_TABLE;
			break;
		default:
			/* Check if there could be more segments */
			if (sqlite3_changes(_db) == 0)
				moreSegments = false;
			else {
				moreSegments = true;
				segnum++;
			}
				
			break;
		}
	}
	
	/* Propagate changes to parent */		
	RecordStore::remove(key);
}

uint64_t
BiometricEvaluation::IO::SQLiteRecordStore::read(
    const std::string &key,
    void *const data)
    const
{
	return (this->readSegments(key, data));
}

void
BiometricEvaluation::IO::SQLiteRecordStore::replace(
    const std::string &key,
    const void *const data,
    const uint64_t size)
{
	if (getMode() == IO::READONLY)
		throw Error::StrategyError("RecordStore was opened read-only");
	if (!validateKeyString(key))
		throw Error::StrategyError("Invalid key format");
	
	/* 
	 * Could perform an UPDATE, but with segmented records, it is much
	 * simpler to remove() all old bits and insert() new ones.
	 */
	this->remove(key);
	this->insert(key, data, size);
}

uint64_t
BiometricEvaluation::IO::SQLiteRecordStore::length(
    const std::string &key)
    const
{
	return (this->readSegments(key, nullptr));
}
    
uint64_t
BiometricEvaluation::IO::SQLiteRecordStore::readSegments(
    const std::string &key,
    void * const data)
    const
{	
	if (!validateKeyString(key))
		throw Error::StrategyError("Invalid key format");

	sqlite3_stmt *statement;
	uint64_t segnum = 0;
	uint64_t totalBytes = 0, segBytes;
	std::string activeTable = PRIMARY_KV_TABLE;
	uint8_t *dataPtr = (uint8_t *)data;
	bool moreSegments = true;
	while (moreSegments) {
		std::string sqlCommand = "SELECT " + VALUE_COL + " FROM " + 
		    activeTable + " WHERE " + KEY_COL + " = '" + 
		    genKeySegName(key, segnum) + "' LIMIT 1";
		    
		/* Prepare the statement */
#ifdef	SQLITE_V2_SUPPORT
		int32_t rv = sqlite3_prepare_v2(_db, sqlCommand.c_str(),
		    sqlCommand.length(), &statement, nullptr);
#else
		int32_t rv = sqlite3_prepare(_db, sqlCommand.c_str(),
		    sqlCommand.length(), &statement, nullptr);
#endif
		if (rv != SQLITE_OK) {
			sqlite3_finalize(statement);
			sqliteError(rv);
		}
		if (statement == nullptr)
			throw Error::StrategyError("SQLite: Could not allocate "
			    "statement");
			
		/* Execute the statement */
		rv = sqlite3_step(statement);
		switch (segnum) {
		case 0:
			if (rv != SQLITE_ROW) {
				sqlite3_finalize(statement);
				throw Error::ObjectDoesNotExist(key);
			}
			/* FALLTHROUGH */
		default:
			segBytes = sqlite3_column_bytes(statement, 0);
			totalBytes += segBytes;
			
			if (data != nullptr) {
				memcpy(dataPtr,
				    sqlite3_column_blob(statement, 0),
				    segBytes);
				dataPtr += segBytes;
				break;
			}
		}

		/* Free the statement */
		rv = sqlite3_finalize(statement);
		if (rv != SQLITE_OK)
			sqliteError(rv);
			
		/* Increment segment number if there's more data */
		if (segBytes == MAX_REC_SIZE) {
			switch (segnum) {
			case 0:
				segnum = KEY_SEGMENT_START;
				activeTable = SUBORDINATE_KV_TABLE;
				break;
			default:
				segnum++;
				break;
			}
			moreSegments = true;
		} else
			moreSegments = false;
	}
		
	return (totalBytes);
}
			    
void
BiometricEvaluation::IO::SQLiteRecordStore::flush(
    const std::string &key)
    const
{
	if (!validateKeyString(key))
		throw Error::StrategyError("Invalid key format");

	/* 
	 * SQLite performs an fsync() at the end of every transaction and this
	 * cannot be forced at other times.  We can create another transaction
	 * and ensure the key exists by checking its length.
	 */
	this->length(key);
}

uint64_t
BiometricEvaluation::IO::SQLiteRecordStore::sequence(
    std::string &key,
    void *const data,
    int cursor)
{
	if ((cursor != BE_RECSTORE_SEQ_START) &&
	    (cursor != BE_RECSTORE_SEQ_NEXT))
		throw Error::StrategyError("Invalid cursor position as " 
		    "argument");
		    
	int32_t rv;
	if ((cursor == BE_RECSTORE_SEQ_START) || (_sequencer == nullptr)) {
		std::string sqlCommand = "SELECT *,ROWID FROM " + 
		    PRIMARY_KV_TABLE + " " + "ORDER BY ROWID";
		
		/* Finalize previous statement */
		rv = sqlite3_finalize(_sequencer);
		if (rv != SQLITE_OK)
			sqliteError(rv);
		
		/* Prepare the statement */
#ifdef	SQLITE_V2_SUPPORT
		rv = sqlite3_prepare_v2(_db, sqlCommand.c_str(),
		    sqlCommand.length(), &_sequencer, nullptr);
#else
		rv = sqlite3_prepare(_db, sqlCommand.c_str(),
		    sqlCommand.length(), &_sequencer, nullptr);
#endif
		if ((rv != SQLITE_OK) || (_sequencer == nullptr))
			sqliteError(rv);
		_sequenceEnd = false;
	}
	
	/* Must reset sequencer first if we've exhausted the list */
	if (_sequenceEnd == true)
		throw Error::ObjectDoesNotExist();
	
	/* 
	 * Must reselect cursor at sequence to protect against other methods
	 * modifying the database between setCursorAtKey() and sequence().
	 */
	if (_cursorRow != 0) {
		/* Finalize previous sequence() SELECT */
		rv = sqlite3_finalize(_sequencer);
		if (rv != SQLITE_OK)
			sqliteError(rv);
	
		std::stringstream sqlCommand;
		sqlCommand << "SELECT *,ROWID FROM " <<
		    PRIMARY_KV_TABLE << " " << "WHERE ROWID >= " << 
		    _cursorRow << " ORDER BY ROWID";
		_cursorRow = 0;
	
		/* Prepare the statement */
#ifdef	SQLITE_V2_SUPPORT
		rv = sqlite3_prepare_v2(_db, sqlCommand.str().c_str(),
		    sqlCommand.str().length(), &_sequencer, nullptr);
#else
		rv = sqlite3_prepare(_db, sqlCommand.str().c_str(),
		    sqlCommand.str().length(), &_sequencer, nullptr);
#endif
		if ((rv != SQLITE_OK) || (_sequencer == nullptr))
			sqliteError(rv);
	}
	
	/* Execute the statement */
	rv = sqlite3_step(_sequencer);
	
	/* End of entries */
	switch (rv) {
	case SQLITE_ROW: {
		key.assign((const char *)sqlite3_column_text(_sequencer, 0));
		uint64_t bytes = sqlite3_column_bytes(_sequencer, 1);
		if (data != nullptr)
			memcpy(data, sqlite3_column_blob(_sequencer, 1), bytes);

		return (bytes);
	} case SQLITE_DONE:
		_sequenceEnd = true;
		throw Error::ObjectDoesNotExist();
		
		/* Not reached */
		return (0);
	default:
		/* Free the statement */
		rv = sqlite3_finalize(_sequencer);
		if (rv != SQLITE_OK)
			sqliteError(rv);
		
		/* Not reached */
		return (0);
	}
}

void
BiometricEvaluation::IO::SQLiteRecordStore::setCursorAtKey(
    const std::string &key)
{
	if (!validateKeyString(key))
		throw Error::StrategyError("Invalid key format");

	int32_t rv;
	
	sqlite3_stmt *statement;
	std::string sqlCommand = "SELECT ROWID FROM " + PRIMARY_KV_TABLE + " " +
	    "WHERE " + KEY_COL + " = '" + key + "'";
	
	/* Prepare the statement */
#ifdef	SQLITE_V2_SUPPORT
	rv = sqlite3_prepare_v2(_db, sqlCommand.c_str(), sqlCommand.length(),
	    &statement, nullptr);
#else
	rv = sqlite3_prepare(_db, sqlCommand.c_str(), sqlCommand.length(),
	    &statement, nullptr);
#endif
	if ((rv != SQLITE_OK) || (statement == nullptr))
		sqliteError(rv);
	
	/* Execute the statement */
	rv = sqlite3_step(statement);
	
	/* End of entries */
	switch (rv) {
	case SQLITE_ROW: {
		_cursorRow = (uint64_t)strtoll(
		    (const char *)sqlite3_column_text(statement, 0), nullptr, 
		    10);

    		rv = sqlite3_finalize(statement);
		if (rv != SQLITE_OK)
			sqliteError(rv);
		break;
	} case SQLITE_DONE:
		rv = sqlite3_finalize(statement);
		if (rv != SQLITE_OK)
			sqliteError(rv);
		throw Error::ObjectDoesNotExist();
		
		/* Not reached */
		break;
	default:
    		rv = sqlite3_finalize(statement);
		if (rv != SQLITE_OK)
			sqliteError(rv);
		throw Error::StrategyError();
		
		/* Not reached */
		break;
	}
		
	/* Reset cursor */
	_sequenceEnd = false;
}

void
BiometricEvaluation::IO::SQLiteRecordStore::cleanup()
{
	int32_t rv;

	/* Finalize sequencer */
	rv = sqlite3_finalize(_sequencer);
	if (rv != SQLITE_OK)
		throw Error::StrategyError("SQLite: Could not finalize "
		    "sequencer");
	_sequenceEnd = false;
	_sequencer = nullptr;
	
	/* Close DB */
	rv = sqlite3_close(_db);
	if (rv != SQLITE_OK) 
		throw Error::StrategyError("SQLite: Busy (did you "
		    "free all statements?)");
}

void
BiometricEvaluation::IO::SQLiteRecordStore::sqliteError(
    int32_t errorNumber)
    const
{	
	std::stringstream msg;
	msg << "sqlite3: " << sqlite3_errmsg(_db) << " (" << errorNumber << ')';
	throw Error::StrategyError(msg.str());
}

void
BiometricEvaluation::IO::SQLiteRecordStore::createStructure()
{
	this->createKeyValueTable(PRIMARY_KV_TABLE);
	this->createKeyValueTable(SUBORDINATE_KV_TABLE);
	
	if (this->validateSchema() == false)
		throw Error::StrategyError("SQLite: Could not validate schema");
}

void
BiometricEvaluation::IO::SQLiteRecordStore::createKeyValueTable(
    const std::string &table)
{
	sqlite3_stmt *statement;
	
	/* Compile the SQL statement */
	std::string sqlCommand = "CREATE TABLE " + table + " " + 
	    "(" + KEY_COL + " VARCHAR(1024) UNIQUE PRIMARY KEY NOT NULL, " +
	    VALUE_COL + " BLOB)";
#ifdef	SQLITE_V2_SUPPORT
	int32_t rv = sqlite3_prepare_v2(_db, sqlCommand.c_str(),
	    sqlCommand.length(), &statement, nullptr);
#else
	int32_t rv = sqlite3_prepare(_db, sqlCommand.c_str(),
	    sqlCommand.length(), &statement, nullptr);
#endif
	if (rv != SQLITE_OK) {
		sqlite3_finalize(statement);
		sqliteError(rv);
	}
	if (statement == nullptr)
		throw Error::StrategyError("SQLite: Could not allocate "
		    "statement");
		
	/* Execute the statement */
	rv = sqlite3_step(statement);
	if (rv != SQLITE_DONE) {
		sqlite3_finalize(statement);
		sqliteError(rv);
	}
		
	/* Free the statement */
	rv = sqlite3_finalize(statement);
	if (rv != SQLITE_OK)
		sqliteError(rv);
}

bool
BiometricEvaluation::IO::SQLiteRecordStore::validateSchema()
{
	return (this->validateKeyValueTable(PRIMARY_KV_TABLE) &&
	    this->validateKeyValueTable(SUBORDINATE_KV_TABLE));
}

bool
BiometricEvaluation::IO::SQLiteRecordStore::validateKeyValueTable(
    const std::string &table)
{
	sqlite3_stmt *statement;

	/* Schema is good if there exists key and value columns */
	std::string sqlCommand = "SELECT " + KEY_COL + "," + VALUE_COL + 
	    " FROM " + table + " LIMIT 1";
#ifdef	SQLITE_V2_SUPPORT
	int32_t rv = sqlite3_prepare_v2(_db, sqlCommand.c_str(),
	    sqlCommand.length(), &statement, nullptr);
#else
	int32_t rv = sqlite3_prepare(_db, sqlCommand.c_str(),
	    sqlCommand.length(), &statement, nullptr);
#endif
	if ((rv != SQLITE_OK) || (statement == nullptr))
		sqliteError(rv);

	/* Execute the statement */
	bool valid = false;
	rv = sqlite3_step(statement);
	if (rv == SQLITE_DONE || rv == SQLITE_ROW)
		valid = true;
		
	/* Free the statement */
	rv = sqlite3_finalize(statement);
	if (rv != SQLITE_OK)
		sqliteError(rv);
		
	return (valid);
}

