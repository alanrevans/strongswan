/*
 * Copyright (C) 2011-2012 Sansar Choinyambuu, Andreas Steffen
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

#include "imv_attestation_process.h"

#include <ietf/ietf_attr_pa_tnc_error.h>

#include <pts/pts.h>

#include <tcg/tcg_pts_attr_aik.h>
#include <tcg/tcg_pts_attr_dh_nonce_params_resp.h>
#include <tcg/tcg_pts_attr_file_meas.h>
#include <tcg/tcg_pts_attr_meas_algo.h>
#include <tcg/tcg_pts_attr_proto_caps.h>
#include <tcg/tcg_pts_attr_simple_comp_evid.h>
#include <tcg/tcg_pts_attr_simple_evid_final.h>
#include <tcg/tcg_pts_attr_tpm_version_info.h>
#include <tcg/tcg_pts_attr_unix_file_meta.h>

#include <debug.h>
#include <crypto/hashers/hasher.h>

#include <inttypes.h>

bool imv_attestation_process(pa_tnc_attr_t *attr, linked_list_t *attr_list,
							 imv_attestation_state_t *attestation_state,
							 pts_meas_algorithms_t supported_algorithms,
							 pts_dh_group_t supported_dh_groups,
							 pts_database_t *pts_db,
							 credential_manager_t *pts_credmgr)
{
	pen_type_t attr_type;
	pts_t *pts;
	
	pts = attestation_state->get_pts(attestation_state);
	attr_type = attr->get_type(attr);

	switch (attr_type.type)
	{
		case TCG_PTS_PROTO_CAPS:
		{
			tcg_pts_attr_proto_caps_t *attr_cast;
			pts_proto_caps_flag_t flags;

			attr_cast = (tcg_pts_attr_proto_caps_t*)attr;
			flags = attr_cast->get_flags(attr_cast);
			pts->set_proto_caps(pts, flags);
			break;
		}
		case TCG_PTS_MEAS_ALGO_SELECTION:
		{
			tcg_pts_attr_meas_algo_t *attr_cast;
			pts_meas_algorithms_t selected_algorithm;

			attr_cast = (tcg_pts_attr_meas_algo_t*)attr;
			selected_algorithm = attr_cast->get_algorithms(attr_cast);
			if (!(selected_algorithm & supported_algorithms))
			{
				DBG1(DBG_IMV, "PTS-IMC selected unsupported"
							  " measurement algorithm");
				return FALSE;
			}
			pts->set_meas_algorithm(pts, selected_algorithm);
			break;
		}
		case TCG_PTS_DH_NONCE_PARAMS_RESP:
		{
			tcg_pts_attr_dh_nonce_params_resp_t *attr_cast;
			int nonce_len, min_nonce_len;
			pts_dh_group_t dh_group;
			pts_meas_algorithms_t offered_algorithms, selected_algorithm;
			chunk_t responder_value, responder_nonce;

			attr_cast = (tcg_pts_attr_dh_nonce_params_resp_t*)attr;
			responder_nonce = attr_cast->get_responder_nonce(attr_cast);

			/* check compliance of responder nonce length */
			min_nonce_len = lib->settings->get_int(lib->settings,
						"libimcv.plugins.imv-attestation.min_nonce_len", 0);
			nonce_len = responder_nonce.len;
			if (nonce_len < PTS_MIN_NONCE_LEN ||
			   (min_nonce_len > 0 && nonce_len < min_nonce_len))
			{
				attr = pts_dh_nonce_error_create(
									max(PTS_MIN_NONCE_LEN, min_nonce_len),
										PTS_MAX_NONCE_LEN);
				attr_list->insert_last(attr_list, attr);
				break;
			}

			dh_group = attr_cast->get_dh_group(attr_cast);
			if (!(dh_group & supported_dh_groups))
			{
				DBG1(DBG_IMV, "PTS-IMC selected unsupported DH group");
				return FALSE;
			}

			offered_algorithms = attr_cast->get_hash_algo_set(attr_cast);
			selected_algorithm = pts_meas_algo_select(supported_algorithms,
													  offered_algorithms);
			if (selected_algorithm == PTS_MEAS_ALGO_NONE)
			{
				attr = pts_hash_alg_error_create(supported_algorithms);
				attr_list->insert_last(attr_list, attr);
				break;
			}
			pts->set_dh_hash_algorithm(pts, selected_algorithm);

			if (!pts->create_dh_nonce(pts, dh_group, nonce_len))
			{
				return FALSE;
			}

			responder_value = attr_cast->get_responder_value(attr_cast);
			pts->set_peer_public_value(pts, responder_value,
											responder_nonce);

			/* Calculate secret assessment value */
			if (!pts->calculate_secret(pts))
			{
				return FALSE;
			}
			break;
		}
		case TCG_PTS_TPM_VERSION_INFO:
		{
			tcg_pts_attr_tpm_version_info_t *attr_cast;
			chunk_t tpm_version_info;

			attr_cast = (tcg_pts_attr_tpm_version_info_t*)attr;
			tpm_version_info = attr_cast->get_tpm_version_info(attr_cast);
			pts->set_tpm_version_info(pts, tpm_version_info);
			break;
		}
		case TCG_PTS_AIK:
		{
			tcg_pts_attr_aik_t *attr_cast;
			certificate_t *aik, *issuer;
			public_key_t *public;
			chunk_t keyid;
			enumerator_t *e;
			bool trusted = FALSE;

			attr_cast = (tcg_pts_attr_aik_t*)attr;
			aik = attr_cast->get_aik(attr_cast);
			if (!aik)
			{
				DBG1(DBG_IMV, "AIK unavailable");
				return FALSE;
			}
			if (aik->get_type(aik) == CERT_X509)
			{
				public = aik->get_public_key(aik);
				public->get_fingerprint(public, KEYID_PUBKEY_INFO_SHA1, &keyid);
				DBG1(DBG_IMV, "verifying AIK certificate with keyid %#B", &keyid);
				public->destroy(public);

				e = pts_credmgr->create_trusted_enumerator(pts_credmgr,
							KEY_ANY, aik->get_issuer(aik), FALSE);
				while (e->enumerate(e, &issuer))
				{
					if (aik->issued_by(aik, issuer, NULL))
					{
						trusted = TRUE;
						break;
					}
				}
				e->destroy(e);
				DBG1(DBG_IMV, "AIK certificate is %strusted",
							   trusted ? "" : "not ");
				if (!trusted)
				{
					return FALSE;
				}
			}
			pts->set_aik(pts, aik);
			break;
		}
		case TCG_PTS_FILE_MEAS:
		{
			tcg_pts_attr_file_meas_t *attr_cast;
			u_int16_t request_id;
			int file_count, file_id;
			pts_meas_algorithms_t algo;
			pts_file_meas_t *measurements;
			char *platform_info;
			enumerator_t *e_hash;
			bool is_dir;

			platform_info = pts->get_platform_info(pts);
			if (!pts_db || !platform_info)
			{
				DBG1(DBG_IMV, "%s%s%s not available",
					(pts_db) ? "" : "pts database",
					(!pts_db && !platform_info) ? "and" : "",
					(platform_info) ? "" : "platform info");
				break;
			}

			attr_cast = (tcg_pts_attr_file_meas_t*)attr;
			measurements = attr_cast->get_measurements(attr_cast);
			algo = pts->get_meas_algorithm(pts);
			request_id = measurements->get_request_id(measurements);
			file_count = measurements->get_file_count(measurements);

			DBG1(DBG_IMV, "measurement request %d returned %d file%s:",
				 request_id, file_count, (file_count == 1) ? "":"s");

			if (request_id)
			{
				if (!attestation_state->check_off_file_meas_request(
					attestation_state, request_id, &file_id, &is_dir))
				{
					DBG1(DBG_IMV, "  no entry found for file measurement "
								  "request %d", request_id);
					break;
				}

				/* check hashes from database against measurements */
				e_hash = pts_db->create_file_hash_enumerator(pts_db,
								platform_info, algo, file_id, is_dir);
				if (!measurements->verify(measurements, e_hash, is_dir))
				{
					attestation_state->set_measurement_error(attestation_state);
				}
				e_hash->destroy(e_hash);
			}
			else
			{
				measurements->check(measurements, pts_db, platform_info, algo);
			}
			break;
		}
		case TCG_PTS_UNIX_FILE_META:
		{
			tcg_pts_attr_file_meta_t *attr_cast;
			int file_count;
			pts_file_meta_t *metadata;
			pts_file_metadata_t *entry;
			time_t created, modified, accessed;
			bool utc = FALSE;
			enumerator_t *e;

			attr_cast = (tcg_pts_attr_file_meta_t*)attr;
			metadata = attr_cast->get_metadata(attr_cast);
			file_count = metadata->get_file_count(metadata);

			DBG1(DBG_IMV, "metadata request returned %d file%s:",
				 file_count, (file_count == 1) ? "":"s");

			e = metadata->create_enumerator(metadata);
			while (e->enumerate(e, &entry))
			{
				DBG1(DBG_IMV, " '%s' (%"PRIu64" bytes)"
							  " owner %"PRIu64", group %"PRIu64", type %N",
					 entry->filename, entry->filesize, entry->owner,
					 entry->group, pts_file_type_names, entry->type);

				created = entry->created;
				modified = entry->modified;
				accessed = entry->accessed;

				DBG1(DBG_IMV, "    created %T, modified %T, accessed %T",
					 &created, utc, &modified, utc, &accessed, utc);
			}
			e->destroy(e);
			break;
		}
		case TCG_PTS_SIMPLE_COMP_EVID:
		{
			tcg_pts_attr_simple_comp_evid_t *attr_cast;
			pts_comp_func_name_t *name;
			pts_comp_evidence_t *evidence;
			pts_component_t *comp;
			u_int32_t depth;

			attr_cast = (tcg_pts_attr_simple_comp_evid_t*)attr;
			evidence = attr_cast->get_comp_evidence(attr_cast);
			name = evidence->get_comp_func_name(evidence, &depth);

			comp = attestation_state->get_component(attestation_state, name);
			if (!comp)
			{
				DBG1(DBG_IMV, "  no entry found for component evidence request");
				break;
			}
			if (comp->verify(comp, name->get_qualifier(name), pts,
							 evidence) != SUCCESS)
			{
				attestation_state->set_measurement_error(attestation_state);
				name->log(name, "  measurement mismatch for ");
			}
			break;
		}
		case TCG_PTS_SIMPLE_EVID_FINAL:
		{
			tcg_pts_attr_simple_evid_final_t *attr_cast;
			u_int8_t flags;
			pts_meas_algorithms_t comp_hash_algorithm;
			chunk_t pcr_comp, tpm_quote_sig, evid_sig;
			chunk_t pcr_composite, quote_info;
			bool use_quote2, use_ver_info;

			attr_cast = (tcg_pts_attr_simple_evid_final_t*)attr;
			flags = attr_cast->get_quote_info(attr_cast, &comp_hash_algorithm,
											  &pcr_comp, &tpm_quote_sig);

			if (flags != PTS_SIMPLE_EVID_FINAL_NO)
			{
				use_quote2   = (flags == PTS_SIMPLE_EVID_FINAL_QUOTE_INFO2 ||
							    flags == PTS_SIMPLE_EVID_FINAL_QUOTE_INFO2_CAP_VER);
				use_ver_info = (flags == PTS_SIMPLE_EVID_FINAL_QUOTE_INFO2_CAP_VER);

				/* Construct PCR Composite and TPM Quote Info structures */
				if (!pts->get_quote_info(pts, use_quote2, use_ver_info,
						comp_hash_algorithm, &pcr_composite, &quote_info))
				{
					DBG1(DBG_IMV, "unable to construct TPM Quote Info");
					return FALSE;
				}

				if (!chunk_equals(pcr_comp, pcr_composite))
				{
					DBG1(DBG_IMV, "received PCR Composite does not match "
								  "constructed one");
					free(pcr_composite.ptr);
					free(quote_info.ptr);
					return FALSE;
				}
				DBG2(DBG_IMV, "received PCR Composite matches constructed one");
				free(pcr_composite.ptr);

				if (!pts->verify_quote_signature(pts, quote_info, tpm_quote_sig))
				{
					free(quote_info.ptr);
					return FALSE;
				}
				DBG2(DBG_IMV, "TPM Quote Info signature verification successful");
				free(quote_info.ptr);

				/**
				 * Finalize any pending measurement registrations and check
				 * if all expected component measurements were received
				 */
				attestation_state->finalize_components(attestation_state);
			}

			if (attr_cast->get_evid_sig(attr_cast, &evid_sig))
			{
				/** TODO: What to do with Evidence Signature */
				DBG1(DBG_IMV, "this version of the Attestation IMV can not "
							  "handle Evidence Signatures");
			}
			break;
		}

		/* TODO: Not implemented yet */
		case TCG_PTS_INTEG_MEAS_LOG:
		/* Attributes using XML */
		case TCG_PTS_TEMPL_REF_MANI_SET_META:
		case TCG_PTS_VERIFICATION_RESULT:
		case TCG_PTS_INTEG_REPORT:
		/* On Windows only*/
		case TCG_PTS_WIN_FILE_META:
		case TCG_PTS_REGISTRY_VALUE:
		/* Received on IMC side only*/
		case TCG_PTS_REQ_PROTO_CAPS:
		case TCG_PTS_DH_NONCE_PARAMS_REQ:
		case TCG_PTS_DH_NONCE_FINISH:
		case TCG_PTS_MEAS_ALGO:
		case TCG_PTS_GET_TPM_VERSION_INFO:
		case TCG_PTS_REQ_TEMPL_REF_MANI_SET_META:
		case TCG_PTS_UPDATE_TEMPL_REF_MANI:
		case TCG_PTS_GET_AIK:
		case TCG_PTS_REQ_FUNC_COMP_EVID:
		case TCG_PTS_GEN_ATTEST_EVID:
		case TCG_PTS_REQ_FILE_META:
		case TCG_PTS_REQ_FILE_MEAS:
		case TCG_PTS_REQ_INTEG_MEAS_LOG:
		default:
			DBG1(DBG_IMV, "received unsupported attribute '%N'",
				 tcg_attr_names, attr->get_type(attr));
			break;
	}
	return TRUE;
}

