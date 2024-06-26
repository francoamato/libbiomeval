/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

#include <cmath>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <iostream>
#include <be_io_utility.h>
#include <be_io_recordstore.h>
#include <be_data_interchange_an2k.h>

/*
 * This test program exercises the Evaluation framework to process AN2K
 * records stored in a RecordStore. The intent is to model what a real
 * program would do by retrieving AN2K records, doing some processing
 * on the image, and displaying the results.
 */
using namespace std;
using namespace BiometricEvaluation;
using namespace BiometricEvaluation::Framework::Enumeration;

static void
printRecordInfo(const DataInterchange::AN2KRecord &an2k)
{
	cout << "\tVersion: " << an2k.getVersionNumber() << endl;
	cout << "\tDate: " << an2k.getDate() << endl;
	cout << "\tDestination Agency: " <<
	    an2k.getDestinationAgency() << endl;
	cout << "\tOriginating Agency: " <<
	    an2k.getOriginatingAgency() << endl;
	cout << "\tTransaction Control Number: " <<
	    an2k.getTransactionControlNumber() << endl;
	cout << "\tNative Scanning Resolution: " <<
	    an2k.getNativeScanningResolution() << endl;
	cout << "\tNominal Transmitting Resolution: " <<
	    an2k.getNominalTransmittingResolution() << endl;
	cout << "\tCapture Count: " << an2k.getFingerCaptureCount() << endl;
	cout << "\tLatent Count: " << an2k.getFingerLatentCount() << endl;
}

static void
printImageInfo(const Image::Image &img, const string &name,
    const int idx)

{
	cout << "Image info:" << endl;
	cout << "\tCompression: " << to_string(img.getCompressionAlgorithm()) <<
	    endl;
	cout << "\tDimensions: " << img.getDimensions() << endl;
	cout << "\tResolution: " << img.getResolution() << endl;
	cout << "\tDepth: " << img.getColorDepth() << endl;

	ostringstream str;

	str << name << idx << ".pgm";
	string filename = str.str().c_str();
	str.str("");
	str << "P5\n";
	str << "# " << filename << "\n";
	str << img.getDimensions().xSize << " " << img.getDimensions().ySize <<
	    " " << (int)(pow(2.0, (int)img.getColorDepth()) - 1) << "\n";
	ofstream img_out(filename.c_str(), ofstream::binary);
	img_out << str.str();
	Memory::uint8Array imgData{img.getRawData()};
	img_out.write((char *)&(imgData[0]), imgData.size());
	if (img_out.good())
		cout << "\tFile: " << filename << endl;
	else {
		throw (Error::FileError("Image write failure"));
	}
	img_out.close();
}

static void
printViewInfo(const View::AN2KViewVariableResolution &an2kv,
    const string &name, const int idx)
{
	cout << "\tRecord Type: " <<
	    static_cast<std::underlying_type<
	    View::AN2KView::RecordType>::type>(an2kv.getRecordType()) << endl;
	cout << "\tImage resolution: " << an2kv.getImageResolution() << endl;
	cout << "\tImage size: " << an2kv.getImageSize() << endl;
	cout << "\tImage color depth: " << an2kv.getImageColorDepth() << endl;
	cout << "\tCompression: " <<
	    to_string(an2kv.getCompressionAlgorithm()) << endl;
	cout << "\tScan resolution: " << an2kv.getScanResolution() << endl;
	cout << "\tImpression Type: " << to_string(an2kv.getImpressionType()) <<
	    endl;
	cout << "\tSource Agency: " << an2kv.getSourceAgency() << endl;
	cout << "\tCapture Date: " << an2kv.getCaptureDate() << endl;
	cout << "\tComment: [" << an2kv.getComment() << "]" << endl;

	/*
	 * Get the image data.
	 */
	shared_ptr<Image::Image> img = an2kv.getImage();
	if (img != nullptr)
		printImageInfo(*img, name, idx);
	else
		cout << "No Image available." << endl;

	cout << "Get the set of minutiae data records: ";
	vector<Finger::AN2KMinutiaeDataRecord> minutiae =
	    an2kv.getMinutiaeDataRecordSet();
	cout << "There are " << minutiae.size() <<
	    " minutiae data records." << endl;
}

static int
testAN2K11EFS(const std::string &fname)
{
	cout << "Test of Extended Feature Set data in " << fname << ": ";
	try {
		DataInterchange::AN2KRecord an2k(fname);
	std::vector<Finger::AN2KMinutiaeDataRecord> minutiae =
	    an2k.getMinutiaeDataRecordSet();

		Image::ROI roi = minutiae[0].getAN2K11EFS()->getImageInfo().roi;
		cout << "ROI:\n"
		    << "\tSize: ("
		    << roi.size.xSize << "," << roi.size.ySize << ")\n"
		    << "\tOffset: ("
		    << roi.horzOffset << "," << roi.vertOffset << ")\n"
		    << "\tPath: ";
		for (auto const& point: roi.path) {
			cout << point << " ";
		}
		cout << "\n";
	} catch (const Error::Exception &e) {
		cout << "Failed; caught " << e.whatString() << "\n";
		return (1);
	}
	return(0);
}

int
main(int argc, char* argv[]) {

	/*
	 * Open the RecordStore containing the AN2K records.
	 */
	cout << "Opening the Record Store" << endl;
	string rsname = "test_data/AN2KRecordStore";
	std::shared_ptr<IO::RecordStore> rs;
	try {
		rs = IO::RecordStore::openRecordStore(rsname,
		    IO::Mode::ReadOnly);
	} catch (const Error::Exception &e) {
		cout << "Could not open record store " << rsname << ": "
		    << e.what() << endl;
		return (EXIT_FAILURE);
	}

	/*
	 * Read some AN2K records and construct the View objects.
	 */
	BiometricEvaluation::IO::RecordStore::Record record;
	uint64_t count = rs->getCount();
	for (uint64_t c = 0; c < count; c++) {
		try {
			record = rs->sequence();
			cout << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n";
			cout << "AN2K record " << record.key << ":" << endl;

			DataInterchange::AN2KRecord an2k(record.data);
			std::cout << "isAN2KRecord(record.data): ";
			if (DataInterchange::AN2KRecord::isAN2KRecord(
			    record.data))
				std::cout << "[PASS]\n";
			else {
				std::cout << "[FAIL]\n";
				return (EXIT_FAILURE);
			}
			printRecordInfo(an2k);

			int i = 0;
			std::vector<Palm ::AN2KView> palms =
			    an2k.getPalmCaptures();
			for (auto p: palms) {
				cout << "[Palm View " << i <<"]" << endl;
				printViewInfo(p, record.key + ".palm", i++);
				cout << "\tPosition: " << p.getPosition()
				    << endl;
				cout << "[End of Palm View]" << endl;
			}
			i = 0;
			std::vector<Finger::AN2KViewCapture> captures =
			    an2k.getFingerCaptures();
			for (auto c: captures) {
				cout << "[Capture View " << i <<"]" << endl;
				printViewInfo(c, record.key + ".cap", i++);
				cout << "\tPosition: " << c.getPosition()
				    << endl;
				cout << "[End of Capture View]" << endl;
			}
			i = 0;
			std::vector<Latent::AN2KView> latents =
			    an2k.getFingerLatents();
			for (auto l: latents) {
				cout << "[Latent View " << i <<"]" << endl;
				printViewInfo(l, record.key + ".lat", i++);
				cout << "\tPositions: ";
				for (auto p: l.getPositions()) {
					cout << p << " ";
				}
				cout << endl << "[End of Latent View]" << endl;
			}
			std::vector<Finger::AN2KMinutiaeDataRecord> minutiae =
			    an2k.getMinutiaeDataRecordSet();
			cout << minutiae.size() << " minutiae data record(s)";
			if (minutiae.size() > 0) cout << " containing:" << endl;
			else cout << "." << endl;
			for (size_t i = 0; i < minutiae.size(); i++) {
				if (minutiae.at(i).getAN2K7Minutiae().get() !=
				    nullptr)
					cout << "\t* " << minutiae.at(i).
					    getAN2K7Minutiae()->
					    getMinutiaPoints().size();
				else
					cout << "\t* 0";
				cout << " AN2K7 minutiae points" << endl;
			}
			cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n";
		} catch (const Error::Exception &e) {
			cout << "Failed sequence: " << e.what() << endl;
			return (EXIT_FAILURE);
		}
	}

	std::cout << "isAN2KRecord(filename): ";
	if (DataInterchange::AN2KRecord::isAN2KRecord(
	    "test_data/type9-efs.an2k")) {
		if (DataInterchange::AN2KRecord::isAN2KRecord(
		    "test_data/fmr.ansi2004")) {
			std::cout << "[FAIL]\n";
			return (EXIT_FAILURE);
		} else
			std::cout << "[PASS]\n";

	} else {
		std::cout << "[FAIL]\n";
		return (EXIT_FAILURE);
	}

	return(testAN2K11EFS("test_data/type9-efs.an2k"));
}
