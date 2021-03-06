/*
 * Copyright (C) 2012 Andreas Steffen
 * HSR Hochschule fuer Technik Rapperswil
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

/**
 * @defgroup ietf_attr_assess_resultt ietf_attr_assess_result
 * @{ @ingroup ietf
 */

#ifndef IETF_ATTR_ASSESS_RESULT_H_
#define IETF_ATTR_ASSESS_RESULT_H_

typedef struct ietf_attr_assess_result_t ietf_attr_assess_result_t;

#include "ietf_attr.h"
#include "pa_tnc/pa_tnc_attr.h"


/**
 * Class implementing the IETF PA-TNC Assessment Result attribute.
 *
 */
struct ietf_attr_assess_result_t {

	/**
	 * Public PA-TNC attribute interface
	 */
	pa_tnc_attr_t pa_tnc_attribute;

	/**
	 * Get the assessment result
	 *
	 * @return				Assessment Result
	 */
	u_int32_t (*get_result)(ietf_attr_assess_result_t *this);

};

/**
 * Creates an ietf_attr_assess_result_t object
 *
 */
pa_tnc_attr_t* ietf_attr_assess_result_create(u_int32_t result);

/**
 * Creates an ietf_attr_assess_result_t object from received data
 *
 * @param value				unparsed attribute value
 */
pa_tnc_attr_t* ietf_attr_assess_result_create_from_data(chunk_t value);

#endif /** IETF_ATTR_ASSESS_RESULT_H_ @}*/
