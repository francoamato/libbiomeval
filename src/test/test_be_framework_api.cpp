/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

#include <be_framework_api.h>
#include <be_framework_enumeration.h>

#include <iostream>

namespace BE = BiometricEvaluation;
using namespace BE::Framework::Enumeration;

namespace Eval
{
	/*
	 * Custom Status object
	 */
	class Status
	{
	public:
		/** Use enumerations to hide away integer return codes. */
		enum class Code
		{
			Success = 0,
			BadImage,
			BadTemplate,
			VendorDefined
		};

		/**
		 * @brief
		 * Constructor
		 * @note
		 * Default arguments and ordering here are by design. Having the
		 * code first allows someone to return the enumeration and have
		 * an Status implicitly constructed.
		 */
		Status(
		    const Status::Code &code = Code::Success,
		    const std::string &message = "");

		/** @return Status code returned from a method. */
		Code
		getCode()
		    const
		    noexcept
		{
			return (this->code);
		}

		/** @return Information regarding the return from a method. */
		std::string
		getMessage()
		    const
		    noexcept
		{
			return (this->message);
		}

	private:
		/** Status code returned a method. */
		Code code{};
		/** Information regarding the return from the method. */
		std::string message{};
	};

/******************************************************************************
 ******************************************************************************
 ******************************************************************************/

	/*
	 * Namespaced convenience methods.
	 */

	/** Convenience to_string function */
	std::string
	to_string(
	    const Eval::Status &status);

	/** Convenience output stream operator */
	std::ostream&
	operator<<(
	    std::ostream &s,
	    const Eval::Status &status)
	{
		return (s << to_string(status));
	}

/******************************************************************************
 ******************************************************************************
 ******************************************************************************/

	/*
	 * Example API and implementation.
	 */

	/** @return Our custom status */
	inline Status
	createTemplate(
	    uint8_t image)
	{
		/* There are many ways to create a Status object */
		if ((image % 3) == 0)
			return Status(Status::Code::VendorDefined, "");
		else if ((image % 4) == 0)
			return {Status::Code::BadImage, "Low quality"};

		return Status::Code::Success;
	}

	/** @return An integer */
	inline int
	matchTemplates(
	    uint8_t verification,
	    uint8_t enrollment)
	{
		int *foo = nullptr;
		/* Crash */
		*foo = 1;
		return (*foo);
	}

	/** @return A string */
	inline std::string
	getID()
	{
		return {"Fingerprint Matcher 1.0"};
	}
}

BE_FRAMEWORK_ENUMERATION_DECLARATIONS(
   Eval::Status::Code,
   Eval_Status_Code_EnumToStringMap);

const std::map<Eval::Status::Code, std::string>
Eval_Status_Code_EnumToStringMap = {
	{Eval::Status::Code::Success, "Success"},
	{Eval::Status::Code::BadImage, "Bad Image"},
	{Eval::Status::Code::BadTemplate, "Bad Template"},
	{Eval::Status::Code::VendorDefined, "Vendor Defined"},
};
BE_FRAMEWORK_ENUMERATION_DEFINITIONS(
   Eval::Status::Code,
   Eval_Status_Code_EnumToStringMap);

Eval::Status::Status(
    const Status::Code &code,
    const std::string &message) :
    code(code),
    message(message)
{

}

std::string
Eval::to_string(
    const Eval::Status &status)
{
	std::string s{BE::Framework::Enumeration::to_string(status.getCode())};

	const auto message = status.getMessage();
	if (!message.empty())
		s += " (" + message + ")";

	return (s);
}

/******************************************************************************
 ******************************************************************************
 ******************************************************************************/

int
main()
{
	for (uint8_t i = 0; i < 10; ++i) {
		const auto status = Eval::createTemplate(i);
		std::cout << "Returned: " << status << std::endl;
		std::cout << "\tCode: " << to_string(status.getCode()) <<
		    " (" << to_int_type(status.getCode()) << ")\n";
		const auto message = status.getMessage();
		std::cout << "\tMessage: " <<
		    (message.empty() ? "<NO MESSAGE>" : message) << '\n';
	}

	BE::Framework::API<Eval::Status> API1;

	/* Can have success and failure block */
	const auto result1 = API1.call([&]() -> Eval::Status {
		return (Eval::createTemplate(1));
	},

	/* Success */
	[&](const BE::Framework::API<Eval::Status>::Result &result) -> void {
		std::cout << "Completed in " <<
		    result.elapsed<std::chrono::microseconds>() << "µs, with "
		    "return value of " <<
		    to_string(result.status) << std::endl;
	},

	/* Failure */
	[&](const BE::Framework::API<Eval::Status>::Result &result) -> void {
		std::cout << "Failed in " <<
		    result.elapsed<std::chrono::microseconds>() << "µs, with "
		    "reason " << to_string(result.currentState) << std::endl;
	});

	/* ...or just the operation, and check success/failure separately */
	const auto result2 = API1.call([&]() -> Eval::Status {
		return (Eval::createTemplate(1));
	});
	std::cout << (result2 ? to_string(result2.status) :
	    to_string(result2.currentState)) << std::endl;

	/* We can also use this for API methods that don't use our object */
	BE::Framework::API<std::string> stringAPI;
	const auto stringResult = stringAPI.call([&]() -> std::string {
		return (Eval::getID());
	});
	/* stringResult.status is a string */
	std::cout << "Returned '" << stringResult.status << "' in " <<
	    stringResult.elapsed<std::chrono::microseconds>() << "µs\n";

	/*
	 * call() returns an int, but we don't have to capture it if we use
	 * lambda callbacks (the same status is available in the lambdas).
	 */
	BE::Framework::API<int> intAPI;
	intAPI.call([&]() -> int {
		return (Eval::matchTemplates(1, 1));},
	[&](const BE::Framework::API<int>::Result &result) {
		std::cout << "Successful match (not expected)" << std::endl;
		std::cout << "Status was " << result.status << std::endl;
	}, [&](const BE::Framework::API<int>::Result &result) {
		std::cout << "Current state: " <<
		    to_string(result.currentState) << std::endl;
		std::cout << "Failed match (as expected)" << std::endl;
	});

	/* You can also use anonymous objects for shorthand (but don't) */
	if (BE::Framework::API<int>().call([&]() -> int { return (
		Eval::matchTemplates(1, 1));
	}))
		std::cout << "Operation completed (not expected)" << std::endl;
	else
		std::cout << "Operation failed (as expected)" << std::endl;

	/* Modify the API helper elements directly. */
	intAPI.getSignalManager()->setDefaultSignalSet();
	intAPI.getWatchdog()->setInterval(30 *
	    BE::Time::MicrosecondsPerSecond);

	return (0);
}

