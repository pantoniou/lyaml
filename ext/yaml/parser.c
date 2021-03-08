/*
 * parser.c, libyaml parser binding for Lua
 * Written by Gary V. Vaughan, 2013
 *
 * Copyright (C) 2013-2020 Gary V. Vaughan
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "lyaml.h"

typedef struct {
   lua_State	 *L;
   struct fy_parser *parser;
   struct fy_event *event;
   int		  document_count;
} lyaml_parser;


static void
parser_delete_event (lyaml_parser *parser)
{
   if (parser->event)
   {
      fy_parser_event_free(parser->parser, parser->event);
      parser->event = NULL;
   }
}

/* With the event result table on the top of the stack, insert
   a mark entry. */
static void
parser_set_mark (lua_State *L, const char *k, const struct fy_mark *mark)
{
   lua_pushstring  (L, k);
   lua_createtable (L, 0, 3);
#define MENTRY(_s)	RAWSET_INTEGER(#_s, mark ? mark->_s : 0)
        RAWSET_INTEGER( "index", mark ? mark->input_pos : 0);
        // MENTRY( index	);
        MENTRY( line	);
        MENTRY( column	);
#undef MENTRY
   lua_rawset (L, -3);
}

/* Push a new event table, pre-populated with shared elements. */
static void
parser_push_eventtable (lyaml_parser *parser, const char *v, int n)
{
   lua_State *L = parser->L;

   lua_createtable (L, 0, n + 3);
   RAWSET_STRING   ("type", v);
#define MENTRY(_s)	parser_set_mark (L, #_s, fy_event_ ## _s (parser->event))
        MENTRY( start_mark	);
        MENTRY( end_mark	);
#undef MENTRY
}

static void
parse_STREAM_START (lyaml_parser *parser)
{
#define EVENTF(_f)	(parser->event.data.stream_start._f)
   lua_State *L = parser->L;

   parser_push_eventtable (parser, "STREAM_START", 1);
   RAWSET_STRING ("encoding", "UTF8");	/* libfyaml only supports UTF8 */
#undef EVENTF
}

static void
parse_DOCUMENT_START (lyaml_parser *parser)
{
#define EVENTF(_f)	(parser->event->document_start._f)
   lua_State *L = parser->L;
   struct fy_document_state *fyds;
   const struct fy_version *version;
   const struct fy_tag *tag;
   void *iter;
   bool first;

   /* increment document count */
   parser->document_count++;

   parser_push_eventtable (parser, "DOCUMENT_START", 1);
   RAWSET_BOOLEAN ("implicit", EVENTF (implicit));

   /* version_directive = { major = M, minor = N } */
   version = fy_document_start_event_version(parser->event);
   if (version)
   {
      lua_pushliteral (L, "version_directive");
      lua_createtable (L, 0, 2);
#define MENTRY(_s)	RAWSET_INTEGER(#_s, version->_s)
        MENTRY( major	);
        MENTRY( minor	);
#undef MENTRY
      lua_rawset (L, -3);
   }

	fyds = parser->event->document_start.document_state;
   iter = NULL;
	if ((tag = fy_document_state_tag_directive_iterate(fyds, &iter)) != NULL)
	{
		lua_pushliteral (L, "tag_directives");
		lua_newtable (L);
		do {
   		lua_createtable (L, 0, 2);
#define MENTRY(_s)	RAWSET_STRING(#_s, tag->_s)
			MENTRY( handle	);
			MENTRY( prefix	);
#undef MENTRY
			lua_rawseti (L, -2, lua_objlen (L, -2) + 1);
		} while ((tag = fy_document_state_tag_directive_iterate(fyds, &iter)) != NULL);
      lua_rawset (L, -3);
	}

#undef EVENTF
}

static void
parse_DOCUMENT_END (lyaml_parser *parser)
{
#define EVENTF(_f)	(parser->event->document_end._f)
   lua_State *L = parser->L;

   parser_push_eventtable (parser, "DOCUMENT_END", 1);
   RAWSET_BOOLEAN ("implicit", EVENTF (implicit));
#undef EVENTF
}

static void
parse_ALIAS (lyaml_parser *parser)
{
#define EVENTF(_f)	(_f)
   lua_State *L = parser->L;
	const char *anchor;

	anchor = fy_token_get_text0(parser->event->alias.anchor);
	if (!anchor) {
         lua_pushfstring (L, "fy_token_get_text0() failed");
         lua_error (L);
	}
   parser_push_eventtable (parser, "ALIAS", 1);
   RAWSET_EVENTF (anchor);
#undef EVENTF
}

static void
parse_SCALAR (lyaml_parser *parser)
{
#define EVENTF(_f)	(_f)
   lua_State *L = parser->L;
   const char *style;
	const char *anchor;
	const char *tag;
	const char *value;
	enum fy_scalar_style sstyle;

	sstyle = fy_token_scalar_style(parser->event->scalar.value);
   switch (sstyle)
   {
#define MENTRY(_s)		\
      case FYSS_##_s: style = #_s; break

        MENTRY( ANY		);
        MENTRY( PLAIN		);
        MENTRY( SINGLE_QUOTED	);
        MENTRY( DOUBLE_QUOTED	);
        MENTRY( LITERAL		);
        MENTRY( FOLDED		);
#undef MENTRY

      default:
         lua_pushfstring (L, "invalid sequence style %d", EVENTF (sstyle));
         lua_error (L);
   }

	anchor = fy_token_get_text0(parser->event->scalar.anchor);
	if (!anchor)
		anchor = "";
	tag = fy_token_get_text0(parser->event->scalar.tag);
	if (!tag)
		tag = "";
	value = fy_token_get_text0(parser->event->scalar.value);
	if (!value)
		value = "";

   parser_push_eventtable (parser, "SCALAR", 6);
   RAWSET_EVENTF (anchor);
   RAWSET_EVENTF (tag);
   RAWSET_EVENTF (value);

   RAWSET_BOOLEAN ("plain_implicit", false);	/* those two are not in fyaml */
   RAWSET_BOOLEAN ("quoted_implicit", false);
   RAWSET_STRING  ("style", style);
#undef EVENTF
}

static void
parse_SEQUENCE_START (lyaml_parser *parser)
{
#define EVENTF(_f)	(_f)
   lua_State *L = parser->L;
	enum fy_node_style nstyle;
	const char *anchor;
	const char *tag;
	bool implicit;
   const char *style;

	nstyle = fy_event_get_node_style(parser->event);
   switch (nstyle)
   {
#define MENTRY(_s)		\
      case FYNS_##_s : style = #_s; break

        MENTRY( ANY	);
        MENTRY( BLOCK	);
        MENTRY( FLOW	);
#undef MENTRY

      default:
         lua_pushfstring (L, "invalid sequence style %d", nstyle);
         lua_error (L);
   }

	anchor = fy_token_get_text0(parser->event->sequence_start.anchor);
	if (!anchor)
		anchor = "";
	tag = fy_token_get_text0(parser->event->sequence_start.tag);
	if (!tag)
		tag = "";
   implicit = parser->event->sequence_start.sequence_start == NULL;	

   parser_push_eventtable (parser, "SEQUENCE_START", 4);
   RAWSET_EVENTF (anchor);
   RAWSET_EVENTF (tag);
   RAWSET_BOOLEAN ("implicit", EVENTF (implicit));
   RAWSET_STRING ("style", style);
#undef EVENTF
}

static void
parse_MAPPING_START (lyaml_parser *parser)
{
#define EVENTF(_f)	(_f)
   lua_State *L = parser->L;
	enum fy_node_style nstyle;
	const char *anchor;
	const char *tag;
	bool implicit;
   const char *style;

	nstyle = fy_event_get_node_style(parser->event);
   switch (nstyle)
   {
#define MENTRY(_s)		\
      case FYNS_##_s : style = #_s; break

        MENTRY( ANY	);
        MENTRY( BLOCK	);
        MENTRY( FLOW	);
#undef MENTRY

      default:
         lua_pushfstring (L, "invalid mapping style %d", nstyle);
         lua_error (L);
   }

	anchor = fy_token_get_text0(parser->event->mapping_start.anchor);
	if (!anchor)
		anchor = "";
	tag = fy_token_get_text0(parser->event->mapping_start.tag);
	if (!tag)
		tag = "";
   implicit = parser->event->mapping_start.mapping_start == NULL;	

   parser_push_eventtable (parser, "MAPPING_START", 4);
   RAWSET_EVENTF (anchor);
   RAWSET_EVENTF (tag);
   RAWSET_BOOLEAN ("implicit", EVENTF (implicit));
   RAWSET_STRING ("style", style);
#undef EVENTF
}

static void
parser_generate_error_message (lyaml_parser *parser)
{
   struct fy_parser *P = parser->parser;
   char buf[256];
   luaL_Buffer b;

   luaL_buffinit (parser->L, &b);
#if 0
   luaL_addstring (&b, P->problem ? P->problem : "A problem");
#else
   luaL_addstring (&b, "A problem");
#endif
   snprintf (buf, sizeof (buf), " at document: %d", parser->document_count);
   luaL_addstring (&b, buf);

#if 0
   if (P->problem_mark.line || P->problem_mark.column)
   {
      snprintf (buf, sizeof (buf), ", line: %lu, column: %lu",
         (unsigned long) P->problem_mark.line + 1,
         (unsigned long) P->problem_mark.column + 1);
      luaL_addstring (&b, buf);
   }
   luaL_addstring (&b, "\n");

   if (P->context)
   {
      snprintf (buf, sizeof (buf), "%s at line: %lu, column: %lu\n",
         P->context,
         (unsigned long) P->context_mark.line + 1,
         (unsigned long) P->context_mark.column + 1);
      luaL_addstring (&b, buf);
   }
#endif

   luaL_pushresult (&b);
}

static int
event_iter (lua_State *L)
{
   lyaml_parser *parser = (lyaml_parser *)lua_touserdata(L, lua_upvalueindex(1));
   char *str;

   parser_delete_event (parser);
   parser->event = fy_parser_parse(parser->parser);
   if (!parser->event)
   {
      parser_generate_error_message (parser);
      return lua_error (L);
   }

   lua_newtable    (L);
   lua_pushliteral (L, "type");

   switch (parser->event->type)
   {
      /* First the simple events, generated right here... */
#define MENTRY(_s)		\
      case FYET_##_s : parser_push_eventtable (parser, #_s, 0); break
         MENTRY( STREAM_END	);
         MENTRY( SEQUENCE_END   );
         MENTRY( MAPPING_END	);
#undef MENTRY

      /* ...then the complex events, generated by a function call. */
#define MENTRY(_s)		\
      case FYET_##_s : parse_##_s (parser); break
         MENTRY( STREAM_START	);
         MENTRY( DOCUMENT_START	);
         MENTRY( DOCUMENT_END	);
         MENTRY( ALIAS		);
         MENTRY( SCALAR		);
         MENTRY( SEQUENCE_START	);
         MENTRY( MAPPING_START	);
#undef MENTRY

      case FYET_NONE:
         lua_pushnil (L);
         break;
      default:
         lua_pushfstring  (L, "invalid event %d", parser->event->type);
         return lua_error (L);
   }

   return 1;
}

static int
parser_gc (lua_State *L)
{
   lyaml_parser *parser = (lyaml_parser *) lua_touserdata (L, 1);

   if (parser)
   {
      parser_delete_event (parser);
      if (parser->parser)
         fy_parser_destroy(parser->parser);
   }
   return 0;
}

void
parser_init (lua_State *L)
{
   luaL_newmetatable(L, "lyaml.parser");
   lua_pushcfunction(L, parser_gc);
   lua_setfield(L, -2, "__gc");
}

int
Pparser (lua_State *L)
{
   static const struct fy_parse_cfg cfg = {
	.search_path = "",
	.flags = FYPCF_QUIET | FYPCF_DEBUG_DEFAULT | FYPCF_DEBUG_LEVEL_WARNING,
   };
   lyaml_parser *parser;
   const unsigned char *str;

   /* requires a single string type argument */
   luaL_argcheck (L, lua_isstring (L, 1), 1, "must provide a string argument");
   str = (const unsigned char *) lua_tostring (L, 1);

   /* create a user datum to store the parser */
   parser = (lyaml_parser *) lua_newuserdata (L, sizeof (*parser));
   memset ((void *) parser, 0, sizeof (*parser));
   parser->L = L;

   /* set its metatable */
   luaL_getmetatable (L, "lyaml.parser");
   lua_setmetatable  (L, -2);

   /* try to initialize the parser */

   parser->parser = fy_parser_create(&cfg);
   if (!parser->parser)
      luaL_error (L, "cannot initialize parser for %s", str);
   fy_parser_set_string(parser->parser, str, lua_strlen (L, 1));

   /* create and return the iterator function, with the loader userdatum as
      its sole upvalue */
   lua_pushcclosure (L, event_iter, 1);

   return 1;
}
