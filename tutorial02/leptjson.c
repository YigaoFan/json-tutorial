#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL, strtod() */
#include <string.h> /* strlen() */
#include <math.h> /* HUGE_VAL */

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)

typedef struct {
	const char* json;
}lept_context;

static void lept_parse_whitespace(lept_context* c) {
	const char *p = c->json;
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
		p++;
	c->json = p;
}

static int lept_parse_literal(lept_context* c, lept_value* v, const char* sample, lept_type dest) {
	int i, bound, len = strlen(sample);

	EXPECT(c, sample[0]);
	for(i = 0, bound = len - 2; i<=bound; i++) 
	{
		if (c->json[i] != sample[i+1]) 
			return LEPT_PARSE_INVALID_VALUE;
	}
	c->json += len-1;
	v->type = dest;
	return LEPT_PARSE_OK;
}

#define ISDIGIT(ch) ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch) ((ch) >= '1' && (ch) <= '9')
static int lept_parse_number(lept_context* c, lept_value* v) {
	char *end, *num = (char *)c->json; 
	/* \TODO validate number */

	int i, len;
	for(i=0, len = strlen(num); i<len; i++)
	{
		if (!ISDIGIT(*num) && *num!='+' && *num!='-' && *num!='e' && *num!='E') 
			return LEPT_PARSE_INVALID_VALUE;	
	}

	/* check symbol */
	if(!ISDIGIT(*num)) {
		if(*num != '-') return LEPT_PARSE_INVALID_VALUE;
		else ++num;
	}

	if(*num == '0')
		if(ISDIGIT(*(num+1))) return LEPT_PARSE_ROOT_NOT_SINGULAR;
	++num;
	
	while(ISDIGIT(*num)) ++num;

	if(*num != '\0') {
		if(*num == '.') {
			++num;
			/* ensure there is at least one digit */
			if(!ISDIGIT(*num)) return LEPT_PARSE_INVALID_VALUE;
			while(ISDIGIT(*num)) ++num;
			if(*num == 'e' || *num == 'E') {
				++num;
			} else if(*num != '\0') 
				return LEPT_PARSE_INVALID_VALUE; 
		} else if(*num == 'e' || *num == 'E') {
			++num;	
		} else return LEPT_PARSE_INVALID_VALUE; 

		if(*num == '+' || *num == '-') 
			++num;
		while(*num != '\0') {
			if(ISDIGIT(*num)) ++num;
			else return LEPT_PARSE_INVALID_VALUE;
		}
	}

	if((v->n = strtod(c->json, &end)) == HUGE_VAL)
		return LEPT_PARSE_NUMBER_TOO_BIG;
	if (c->json == end)
		return LEPT_PARSE_INVALID_VALUE;

	c->json = end;
	v->type = LEPT_NUMBER;
	return LEPT_PARSE_OK;
}

static int lept_parse_value(lept_context* c, lept_value* v) {
	switch (*c->json) {
		case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
		case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
		case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
		default:   return lept_parse_number(c, v);
		case '\0': return LEPT_PARSE_EXPECT_VALUE;
	}
}

int lept_parse(lept_value* v, const char* json) {
	lept_context c;
	int ret;
	assert(v != NULL);
	c.json = json;
	v->type = LEPT_NULL;
	lept_parse_whitespace(&c);
	if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
		lept_parse_whitespace(&c);
		if (*c.json != '\0') {
			v->type = LEPT_NULL;
			ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
		}
	}
	return ret;
}

lept_type lept_get_type(const lept_value* v) {
	assert(v != NULL);
	return v->type;
}

double lept_get_number(const lept_value* v) {
	assert(v != NULL && v->type == LEPT_NUMBER);
	return v->n;
}
