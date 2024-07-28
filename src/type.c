#include "common.h"

size_t parsety(cnm_t *cnm, ty_t *buf, size_t buflen) {
	size_t len = 0;

	buf[len] = (ty_t){0};
	while (true) {
		if (cnm->tok.ty == TOK_BAND || cnm->tok.ty == TOK_AND) {
			buf[len].cls = TY_REF;
			tok_t lasttok = cnm->tok;
			lasttok.src.len = 1;
			if (cnm->tok.ty == TOK_AND) {
				if (len + 1 >= buflen) {
					cnm->tok = lasttok;
					goto lenerr;
				}
				buf[++len] = (ty_t){ .cls = TY_REF };
				lasttok.src.str++;
			}
			if (tnext(cnm)->ty == TOK_LBK) {
				buf[len].cls = TY_SLICE;
				if (tnext(cnm)->ty != TOK_RBK)
					goto bkerr;
				tnext(cnm);
			}
			if (len + 1 >= buflen) {
				cnm->tok = lasttok;
				goto lenerr;
			}
			buf[++len] = (ty_t){0};
		} else if (cnm->tok.ty == TOK_STAR) {
			buf[len].cls = TY_PTR;
			if (len + 1 >= buflen)
				goto lenerr;
			buf[++len] = (ty_t){0};
			tnext(cnm);
		} else if (strview_eq(cnm->tok.src, SV("mut"))) {
			buf[len].mut = true;
			tnext(cnm);
		} else {
			break;
		}
	}

	int maxint_width = 0;
	switch (cnm->tok.src.str[0]) {
	case 'u':
	case 'i': {
		bool u = cnm->tok.src.str[0] == 'u';
		if (cnm->tok.src.len == 2 && cnm->tok.src.str[1] == '8') {
			maxint_width = 8;
			buf[len++].cls = u ? TY_U8 : TY_I8;
			goto dointwidth;
		} else if (cnm->tok.src.len == 3 && cnm->tok.src.str[1] == '1'
			&& cnm->tok.src.str[2] == '6') {
			maxint_width = 16;
			buf[len++].cls = u ? TY_U16 : TY_I16;
			goto dointwidth;
		} else if (cnm->tok.src.len == 3 && cnm->tok.src.str[1] == '3'
			&& cnm->tok.src.str[2] == '2') {
			maxint_width = 32;
			buf[len++].cls = u ? TY_U32 : TY_I32;
			goto dointwidth;
		} else if (cnm->tok.src.len == 3 && cnm->tok.src.str[1] == '6'
			&& cnm->tok.src.str[2] == '4') {
			maxint_width = 64;
			buf[len++].cls = u ? TY_U64 : TY_I64;
			goto dointwidth;
		}
		break;
	}
	case 'f':
		if (cnm->tok.src.len == 3 && cnm->tok.src.str[1] == '3'
			&& cnm->tok.src.str[2] == '2') {
			buf[len++].cls = TY_F32;
			tnext(cnm);
			return len;
		} else if (cnm->tok.src.len == 3 && cnm->tok.src.str[1] == '6'
			&& cnm->tok.src.str[2] == '4') {
			buf[len++].cls = TY_F64;
			tnext(cnm);
			return len;
		}
		break;
	case 'b':
		if (strview_eq(cnm->tok.src, SV("bool"))) {
			buf[len++].cls = TY_BOOL;
			tnext(cnm);
			return len;
		}
		break;
	case 'v':
		if (strview_eq(cnm->tok.src, SV("void"))) {
			buf[len++].cls = TY_VOID;
			tnext(cnm);
			return len;
		}
		break;
	}


	doerr(cnm, 8, "unknown identifier",
		true, &(errinf_t){
		.area = cnm->tok.src,
		.comment = "no identifer with this name is in scope",
		.critical = true,
	}, 1);
	return 0;
dointwidth:
	if (tnext(cnm)->ty != TOK_COLON)
		return len;
	
	// TODO: Parse ct-expression for bit width.

	return len;
bkerr:
	doerr(cnm, 4, "slice types cannot have a pre-defined size since it is calculated at runtime",
		true, &(errinf_t){
		.area = cnm->tok.src,
		.comment = "expected \"]\" here. Remove stuff inbetween the []",
		.critical = true,
	}, 1);
	return 0;

lenerr:
	doerr(cnm, 3, "hit type buffer limit. This is due to cnm type info being stored on the stack sometimes",
		true, &(errinf_t){
		.area = cnm->tok.src,
		.comment = "while parsing this token, the internal type information buffer overflowed",
		.critical = true,
	}, 1);
	return 0;
}

