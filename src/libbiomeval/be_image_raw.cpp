/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

#include <be_image_raw.h>
#include <be_memory_autoarray.h>

BiometricEvaluation::Image::Raw::Raw(
    const uint8_t *data, 
    const uint64_t size, 
    const Size dimensions,
    const unsigned int depth,
    const Resolution resolution) :
    Image(data,
    size,
    dimensions,
    depth,
    resolution,
    CompressionAlgorithm::None)
{

}

BiometricEvaluation::Memory::AutoArray<uint8_t>
BiometricEvaluation::Image::Raw::getRawData()
    const
    throw (Error::DataError)
{
	return (Image::getData());
}

BiometricEvaluation::Memory::AutoArray<uint8_t>
BiometricEvaluation::Image::Raw::getRawGrayscaleData(
    uint8_t depth)
    const
    throw (Error::DataError,
    Error::ParameterError)
{
	return (Image::getRawGrayscaleData(depth));
}

BiometricEvaluation::Image::Raw::~Raw()
{

}
