
#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL, strtod() */
#include <string.h> /* strlen() */
#include <math.h> /* HUGE_VAL */
#include <errno.h>

/* tmp include */
#include <stdio.h>

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch) ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch) ((ch) >= '1' && (ch) <= '9')

/* \TODO 解析数字结尾处的空格可能会有些问题，花时间看一下*/
/* define the stage where the parser is on */
enum {
      ON_NEGATIVE = 0,
      ON_INTEGER_1TO9,
      ON_INTEGER_0,
      ON_FRACTION,
      ON_EXPONENT,
      ON_THE_END,
      ON_ERROR
};
typedef struct {
  const char* json;
}lept_context;

static void lept_parse_whitespace(lept_context* c) {
  const char *p = c->json;
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
    p++;
  /* refresh to c->json */
  c->json = p;
}

static int lept_parse_literal(lept_context* c, lept_value* v, const char* sample, lept_type dest) {
  size_t i, bound, len = strlen(sample);

  EXPECT(c, sample[0]);
  for(i = 0, bound = len - 2; i <= bound; i++) {
    if (c->json[i] != sample[i+1]) {
      return LEPT_PARSE_INVALID_VALUE; 
    }
  } 
  c->json += len-1;
  v->type = dest;
  return LEPT_PARSE_OK;
}

/*记得在handle函数的最后更新c->json*/
int handlePositive(lept_context* c) {
  const char *p = c->json;
  if (*p == '-') {
    p++;
  }
  
  if(ISDIGIT1TO9(*p)) {
    c->json = p;
    return ON_INTEGER_1TO9;
  } else if (*p == '0') {
    c->json = p;
    return ON_INTEGER_0;
  } else {
    return ON_ERROR;
  }
}

int handleInteger1To9(lept_context* c) {
  const char* p = c->json;

  p++;
  while (ISDIGIT(*p)) {
    p++;
  }
  c->json = p;
  
  switch (*p) {
  case '.':
    return ON_FRACTION;
  case 'e':
  case 'E':
    return ON_EXPONENT;
  case 0:
    return ON_THE_END;
  default:
    return ON_ERROR;
  } 
}

int handleInteger0(lept_context* c) {
  const char* p = c->json;
  p++;
  c->json = p;
  
  switch (*p) {
  case '.':
    return ON_FRACTION;
  case 'e':
  case 'E':
    return ON_EXPONENT;
  case 0:
    return ON_THE_END;
  default:
    return ON_ERROR;
  }
}

int handleFraction(lept_context* c) {
  const char* p = c->json;
  p++;
  c->json = p;
  while (ISDIGIT(*p)) {
    p++;
  }
  /*保证.后面必须有一位数字*/
  if (c->json == p) {
    return ON_ERROR;
  }
  c->json = p;
  
  switch (*p) {
  case 'e':
  case 'E':
    return ON_EXPONENT;
  case 0:
    return ON_THE_END;
  default:
    return ON_ERROR;
  }
}

int handleExponent(lept_context* c) {
  const char* p = c->json;
  p++;
  c->json = p;

  if (*p == '+' || *p == '-') {
    p++;
    c->json = p;
  }
  while (ISDIGIT(*p)) {
      p++;
  }
  if (c->json == p) {
    return ON_ERROR;
  }
  
  if (*p != ' ' && *p != 0) {
    return ON_ERROR;
  } else {
    c->json = p;
    return ON_THE_END;
  }
}

static int lept_parse_number(lept_context* c, lept_value* v) {
  char* end;
  const char * p = c->json;
  int stage = ON_NEGATIVE;

  /* stage为ON_THE_END的时候解析的循环结束，循环中如果出现解析错误，直接返回 */
  while(stage != ON_THE_END) {
    switch(stage) {
    default:
    case ON_NEGATIVE:
      /*handle函数中进行下一步状态正误判断*/
      stage = handlePositive(c);/* 某个返回函数解析状态的函数 */
      if (stage == ON_ERROR) {
	v->type = LEPT_NULL;
	return LEPT_PARSE_INVALID_VALUE;
      }
      break;
			  
    case ON_INTEGER_1TO9:
      stage = handleInteger1To9(c);
      if (stage == ON_ERROR) {
	v->type = LEPT_NULL;
  	return LEPT_PARSE_INVALID_VALUE;
      }
      break;
				
    case ON_INTEGER_0:
      stage = handleInteger0(c);
      if (stage == ON_ERROR) {
	v->type = LEPT_NULL;
	return LEPT_PARSE_ROOT_NOT_SINGULAR;
      }
      break;

    case ON_FRACTION:
      stage = handleFraction(c);
      if (stage == ON_ERROR) {
	v->type = LEPT_NULL;
	return LEPT_PARSE_INVALID_VALUE;
      }
      break;

    case ON_EXPONENT:
      stage = handleExponent(c);
      if (stage == ON_ERROR) {
	v->type = LEPT_NULL;
	return LEPT_PARSE_INVALID_VALUE;
      }
      break;
    }
  }

  /*恢复c->json*/
  c->json = p;
  /* \TODO 需要处理字符串转换为数字 */
  errno = 0;
  v->n = strtod(c->json, &end);
  if (errno == ERANGE && (v->n == HUGE_VAL || v->n == -HUGE_VAL)) {
    errno = 0;
    v->type = LEPT_NULL;
    return LEPT_PARSE_NUMBER_TOO_BIG;
  }

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
