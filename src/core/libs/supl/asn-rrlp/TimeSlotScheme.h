/*
 * Generated by asn1c-0.9.22 (http://lionet.info/asn1c)
 * From ASN.1 module "RRLP-Components"
 * 	found in "../rrlp-components.asn"
 */

#ifndef _TimeSlotScheme_H_
#define _TimeSlotScheme_H_


#include <asn_application.h>

/* Including external dependencies */
#include <ENUMERATED.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Dependencies */
typedef enum TimeSlotScheme {
    TimeSlotScheme_equalLength = 0,
    TimeSlotScheme_variousLength = 1
} e_TimeSlotScheme;

/* TimeSlotScheme */
typedef ENUMERATED_t TimeSlotScheme_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_TimeSlotScheme;
asn_struct_free_f TimeSlotScheme_free;
asn_struct_print_f TimeSlotScheme_print;
asn_constr_check_f TimeSlotScheme_constraint;
ber_type_decoder_f TimeSlotScheme_decode_ber;
der_type_encoder_f TimeSlotScheme_encode_der;
xer_type_decoder_f TimeSlotScheme_decode_xer;
xer_type_encoder_f TimeSlotScheme_encode_xer;
per_type_decoder_f TimeSlotScheme_decode_uper;
per_type_encoder_f TimeSlotScheme_encode_uper;

#ifdef __cplusplus
}
#endif

#endif /* _TimeSlotScheme_H_ */
#include <asn_internal.h>
