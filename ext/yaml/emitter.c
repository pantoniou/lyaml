/*
 * emitter.c, LibYAML emitter binding for Lua
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

#include <assert.h>
#include <alloca.h>
#include <stdlib.h>

#include "lyaml.h"


typedef struct {
   struct fy_emitter *emitter;

   /* output accumulator */
   lua_State	   *outputL;
   luaL_Buffer	    yamlbuff;

   /* error handling */
   lua_State	   *errL;
   luaL_Buffer	    errbuff;
   int		    error;
} lyaml_emitter;


/* Emit a STREAM_START event. */
static int
emit_STREAM_START (lua_State *L, lyaml_emitter *emitter)
{
   struct fy_event *fye;
   const char *encoding = NULL;

   RAWGET_STRDUP (encoding); lua_pop (L, 1);

#define MENTRY(_s) (STREQ (encoding, #_s)) { yaml_encoding = YAML_##_s##_ENCODING; }
   if (encoding != NULL && !STREQ(encoding, "UTF8")) 
   {
      emitter->error++;
      luaL_addsize (&emitter->errbuff,
                    sprintf (luaL_prepbuffer (&emitter->errbuff),
                             "invalid stream encoding '%s'", encoding));
   }
#undef MENTRY

   if (encoding) free ((void *) encoding);

	if (emitter->error)
		return -1;

  	return fy_emit_event(emitter->emitter,
					   fy_emit_event_create(emitter->emitter, FYET_STREAM_START));
}


/* Emit a STREAM_END event. */
static int
emit_STREAM_END (lua_State *L, lyaml_emitter *emitter)
{
  	return fy_emit_event(emitter->emitter,
					   fy_emit_event_create(emitter->emitter, FYET_STREAM_END));
}


/* Emit a DOCUMENT_START event. */
static int
emit_DOCUMENT_START (lua_State *L, lyaml_emitter *emitter)
{
	struct fy_version version_directive;
   const struct fy_version *Pversion_directive = NULL;
	int i, tag_count;
	struct fy_tag **tag_directivesp = NULL, *tag_directives = NULL;
	char *handle, *prefix, *handlea, *prefixa;
	int implicit = 1;

	memset(&version_directive, 0, sizeof(version_directive));

   RAWGET_PUSHTABLE ("version_directive");
   if (lua_type (L, -1) == LUA_TTABLE)
   {
      RAWGETS_INTEGER (version_directive.major, "major");
      ERROR_IFNIL ("version_directive missing key 'major'");
      if (emitter->error == 0)
      {
         RAWGETS_INTEGER (version_directive.minor, "minor");
         ERROR_IFNIL ("version_directive missing key 'minor'");
      }
      Pversion_directive = &version_directive;
   }
   lua_pop (L, 1);		/* pop version_directive rawget */

   RAWGET_PUSHTABLE ("tag_directives");
   if (lua_type (L, -1) == LUA_TTABLE)
   {
		tag_count = lua_objlen(L, -1);

		tag_directivesp = alloca((tag_count + 1) * sizeof(*tag_directivesp));
		tag_directives = alloca(tag_count * sizeof(*tag_directives));

      lua_pushnil (L);	/* first key */
		i = 0;
      while (lua_next (L, -2) != 0)
      {
			RAWGET_YAML_CHARP(handle);
         ERROR_IFNIL ("tag_directives item missing key 'handle'");
			handlea = alloca(strlen(handle) + 1);
			strcpy(handlea, handle);
         lua_pop (L, 1);	/* pop handle */

			RAWGET_YAML_CHARP(prefix);
         ERROR_IFNIL ("tag_directives item missing key 'prefix'");
			prefixa = alloca(strlen(prefix) + 1);
			strcpy(prefixa, prefix);
         lua_pop (L, 1);	/* pop prefix */

			tag_directives[i].handle = handlea;
			tag_directives[i].prefix = prefixa;

			tag_directivesp[i] = &tag_directives[i];
			i++;

         /* pop tag_directives list element, leave key for next iteration */
         lua_pop (L, 1);
      }
		tag_directivesp[i++]  = NULL;
   }
   lua_pop (L, 1);	/* pop lua_rawget "tag_directives" result */

   RAWGET_BOOLEAN (implicit); lua_pop (L, 1);

   if (emitter->error != 0)
      return 0;

  	return fy_emit_event(emitter->emitter,
					   fy_emit_event_create(emitter->emitter, FYET_DOCUMENT_START,
							implicit, Pversion_directive, tag_directivesp));
}

/* Emit a DOCUMENT_END event. */
static int
emit_DOCUMENT_END (lua_State *L, lyaml_emitter *emitter)
{
   int implicit = 0, rc;
	struct fy_event *fye;

   RAWGET_BOOLEAN (implicit);

  	return fy_emit_event(emitter->emitter,
					   fy_emit_event_create(emitter->emitter, FYET_DOCUMENT_END,
							implicit));
}


/* Emit a MAPPING_START event. */
static int
emit_MAPPING_START (lua_State *L, lyaml_emitter *emitter)
{
   enum fy_node_style yaml_style;
   char *anchor = NULL, *tag = NULL;
   int implicit = 1;
   const char *style = NULL;

   RAWGET_STRDUP (style);  lua_pop (L, 1);

#define MENTRY(_s) (STREQ (style, #_s)) { yaml_style = FYNS_##_s ; }
   if (style == NULL) { yaml_style = FYNS_ANY; } else
   if MENTRY( BLOCK	) else
   if MENTRY( FLOW	) else
   {
      emitter->error++;
      luaL_addsize (&emitter->errbuff,
                    sprintf (luaL_prepbuffer (&emitter->errbuff),
                             "invalid mapping style '%s'", style));
   }
#undef MENTRY

   RAWGET_YAML_CHARP (anchor); lua_pop (L, 1);
   RAWGET_YAML_CHARP (tag);    lua_pop (L, 1);
   RAWGET_BOOLEAN (implicit);  lua_pop (L, 1);

	(void)implicit;	/* XXX no implicit special */

  	return fy_emit_event(emitter->emitter,
					   fy_emit_event_create(emitter->emitter, FYET_MAPPING_START,
								yaml_style, anchor, tag));
}


/* Emit a MAPPING_END event. */
static int
emit_MAPPING_END (lua_State *L, lyaml_emitter *emitter)
{
  	return fy_emit_event(emitter->emitter,
					   fy_emit_event_create(emitter->emitter, FYET_MAPPING_END));
}


/* Emit a SEQUENCE_START event. */
static int
emit_SEQUENCE_START (lua_State *L, lyaml_emitter *emitter)
{
   enum fy_node_style yaml_style;
   char *anchor = NULL, *tag = NULL;
   int implicit = 1;
   const char *style = NULL;

   RAWGET_STRDUP (style);  lua_pop (L, 1);

#define MENTRY(_s) (STREQ (style, #_s)) { yaml_style = FYNS_##_s ; }
   if (style == NULL) { yaml_style = FYNS_ANY; } else
   if MENTRY( BLOCK	) else
   if MENTRY( FLOW	) else
   {
      emitter->error++;
      luaL_addsize (&emitter->errbuff,
                    sprintf (luaL_prepbuffer (&emitter->errbuff),
                             "invalid sequence style '%s'", style));
   }
#undef MENTRY

   RAWGET_YAML_CHARP (anchor); lua_pop (L, 1);
   RAWGET_YAML_CHARP (tag);    lua_pop (L, 1);
   RAWGET_BOOLEAN (implicit);  lua_pop (L, 1);

	(void)implicit;	/* XXX no implicit special */

  	return fy_emit_event(emitter->emitter,
					   fy_emit_event_create(emitter->emitter, FYET_SEQUENCE_START,
								yaml_style, anchor, tag));
}


/* Emit a SEQUENCE_END event. */
static int
emit_SEQUENCE_END (lua_State *L, lyaml_emitter *emitter)
{
  	return fy_emit_event(emitter->emitter,
					   fy_emit_event_create(emitter->emitter, FYET_SEQUENCE_END));
}


/* Emit a SCALAR event. */
static int
emit_SCALAR (lua_State *L, lyaml_emitter *emitter)
{
   enum fy_scalar_style yaml_style;
   char *anchor = NULL, *tag = NULL, *value;
   int length = 0, plain_implicit = 1, quoted_implicit = 1;
   const char *style = NULL;

   RAWGET_STRDUP (style);  lua_pop (L, 1);

#define MENTRY(_s) (STREQ (style, #_s)) { yaml_style = FYSS_##_s ; }
   if (style == NULL) { yaml_style = FYSS_ANY; } else
   if MENTRY( PLAIN		) else
   if MENTRY( SINGLE_QUOTED	) else
   if MENTRY( DOUBLE_QUOTED	) else
   if MENTRY( LITERAL		) else
   if MENTRY( FOLDED		) else
   {
      emitter->error++;
      luaL_addsize (&emitter->errbuff,
                    sprintf (luaL_prepbuffer (&emitter->errbuff),
                             "invalid scalar style '%s'", style));
   }
#undef MENTRY

   RAWGET_YAML_CHARP (anchor); lua_pop (L, 1);
   RAWGET_YAML_CHARP (tag);    lua_pop (L, 1);
   RAWGET_YAML_CHARP (value);  length = lua_objlen (L, -1); lua_pop (L, 1);
   RAWGET_BOOLEAN (plain_implicit);
   RAWGET_BOOLEAN (quoted_implicit);

	(void)plain_implicit;	/* XXX no such things in libfyaml */
	(void)quoted_implicit;

  	return fy_emit_event(emitter->emitter,
					   fy_emit_event_create(emitter->emitter, FYET_SCALAR,
								yaml_style, value, FY_NT, anchor, tag));
}


/* Emit an ALIAS event. */
static int
emit_ALIAS (lua_State *L, lyaml_emitter *emitter)
{
   char *anchor;

   RAWGET_YAML_CHARP (anchor);

  	return fy_emit_event(emitter->emitter,
					   fy_emit_event_create(emitter->emitter, FYET_ALIAS,
								anchor, FY_NT));
}


static int
emit (lua_State *L)
{
   lyaml_emitter *emitter;
   int rc = -1;
   int finalize = 0;
	const char *type;

   luaL_argcheck (L, lua_istable (L, 1), 1, "expected table");

   emitter = (lyaml_emitter *) lua_touserdata (L, lua_upvalueindex (1));

	RAWGET_STRDUP (type); lua_pop (L, 1);

	if (type == NULL)
	{
		emitter->error++;
		luaL_addstring (&emitter->errbuff, "no type field in event table");
	}
#define MENTRY(_s) (STREQ (type, #_s)) { rc = emit_##_s (L, emitter); }
	/* Minimize comparisons by putting more common types earlier. */
	else if MENTRY( SCALAR		)
	else if MENTRY( MAPPING_START	)
	else if MENTRY( MAPPING_END	)
	else if MENTRY( SEQUENCE_START	)
	else if MENTRY( SEQUENCE_END	)
	else if MENTRY( DOCUMENT_START	)
	else if MENTRY( DOCUMENT_END	)
	else if MENTRY( STREAM_START	)
	else if MENTRY( STREAM_END		)
	else if MENTRY( ALIAS		)
#undef MENTRY
	else
	{
		emitter->error++;
		luaL_addsize (&emitter->errbuff,
						sprintf (luaL_prepbuffer (&emitter->errbuff),
									 "invalid event type '%s'", type));
	}

	/* If the stream has finished, finalize the YAML output. */
	if (type && STREQ (type, "STREAM_END"))
		finalize = 1;

   /* Copy any yaml_emitter_t errors into the error buffer. */
   if (rc)
	{
#if 0
      if (emitter->emitter.problem)
        luaL_addstring (&emitter->errbuff, emitter->emitter.problem);
      else
#endif
      luaL_addsize (&emitter->errbuff,
                    sprintf (luaL_prepbuffer (&emitter->errbuff),
                             "libfyaml emit failed"));

      emitter->error++;
   }

	if (type) free ((void *) type);

   /* Report errors back to the caller as `false, "error message"`. */
   if (emitter->error != 0)
   {
      assert (emitter->error == 1);  /* bail on uncaught additional errors */
      lua_pushboolean (L, 0);
      luaL_pushresult (&emitter->errbuff);
      lua_xmove (emitter->errL, L, 1);
      return 2;
   }

   /* Return `true, "YAML string"` after accepting a STREAM_END event. */
   if (finalize)
   {
      lua_pushboolean (L, 1);
      luaL_pushresult (&emitter->yamlbuff);
      lua_xmove (emitter->outputL, L, 1);
      return 2;
   }

   /* Otherwise, just report success to the caller as `true`. */
   lua_pushboolean (L, 1);
   return 1;
}


static int append_output(struct fy_emitter *emit, enum fy_emitter_write_type type,
								 const char *str, int len, void *userdata)
{
   lyaml_emitter *emitter = (lyaml_emitter *) userdata;

   luaL_addlstring (&emitter->yamlbuff, str, len);
	return len;
}


static int
emitter_gc (lua_State *L)
{
   lyaml_emitter *emitter = (lyaml_emitter *) lua_touserdata (L, 1);

   if (emitter)
		fy_emitter_destroy(emitter->emitter);

   return 0;
}


int
Pemitter (lua_State *L)
{
   lyaml_emitter *emitter;
	struct fy_emitter_cfg cfg;

   lua_newtable (L);	/* object table */

   /* Create a user datum to store the emitter. */
   emitter = (lyaml_emitter *) lua_newuserdata (L, sizeof (*emitter));
   emitter->error = 0;

   /* Initialize the emitter. */
	memset(&cfg, 0, sizeof(cfg));
	cfg.output = append_output;
	cfg.userdata = emitter;
	cfg.flags = FYECF_WIDTH_80 | FYECF_MODE_ORIGINAL | FYECF_INDENT_DEFAULT;
	emitter->emitter = fy_emitter_create(&cfg);
   if (!emitter->emitter)
   {
      return luaL_error (L, "%s", "cannot initialize emitter");
   }
#if 0
   yaml_emitter_set_unicode (&emitter->emitter, 1);
   yaml_emitter_set_width   (&emitter->emitter, 2);
   yaml_emitter_set_output  (&emitter->emitter, &append_output, emitter);
#endif

   /* Set it's metatable, and ensure it is garbage collected properly. */
   luaL_newmetatable (L, "lyaml.emitter");
   lua_pushcfunction (L, emitter_gc);
   lua_setfield      (L, -2, "__gc");
   lua_setmetatable  (L, -2);

   /* Set the emit method of object as a closure over the user datum, and
      return the whole object. */
   lua_pushcclosure (L, emit, 1);
   lua_setfield (L, -2, "emit");

   /* Set up a separate thread to collect error messages; save the thread
      in the returned table so that it's not garbage collected when the
      function call stack for Pemitter is cleaned up.  */
   emitter->errL = lua_newthread (L);
   luaL_buffinit (emitter->errL, &emitter->errbuff);
   lua_setfield (L, -2, "errthread");

   /* Create a thread for the YAML buffer. */
   emitter->outputL = lua_newthread (L);
   luaL_buffinit (emitter->outputL, &emitter->yamlbuff);
   lua_setfield (L, -2, "outputthread");

   return 1;
}
