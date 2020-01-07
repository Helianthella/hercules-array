/**
 * This file is part of Hercules.
 * http://herc.ws - http://github.com/HerculesWS/Hercules
 *
 * Copyright (C) 2019 Hercules Dev Team
 *
 * Hercules is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common/hercules.h"
#include "map/map.h"
#include "map/script.h"

#include "common/db.h"
#include "common/memmgr.h"
#include "common/HPMDataCheck.h"

HPExport struct hplugin_info pinfo = {
	"array",         // Plugin name
	SERVER_TYPE_MAP,     // Which server types this plugin works with?
	"0.1.0",             // Plugin version
	HPM_VERSION,         // HPM Version (don't change, macro is automatically updated)
};

static int script_array_index_rcmp(const void *a, const void *b)
{
	return (*(const unsigned int *)b - *(const unsigned int *)a); // scary type conversion
}

static int script_array_find(struct script_state *st, struct map_session_data *sd, const char *name, uint32 start, struct reg_db *ref, const void *needle, bool reverse, bool neq)
{
	struct reg_db *src = script->array_src(st, sd, name, ref);

	if (src != NULL && src->arrays) {
		int ref_id = script->add_word(name);
		struct script_array *sa = idb_get(src->arrays, ref_id);

		if (sa == NULL) {
			return -1;
		}

		unsigned int *list = script->array_cpy_list(sa);
		unsigned int size = sa->size;
		qsort(list, size, sizeof(unsigned int), reverse ? script_array_index_rcmp : script_array_index_cmp);

		if (size >= 1 && is_string_variable(name)) {
			for (unsigned int i = 0; i < size; i++) {
				if (list[i] < start) {
					continue;
				}

				int64 uid = reference_uid(ref_id, list[i]);
				const char *tmp = script->get_val2(st, uid, ref);
				bool matches = strcmp((const char *)needle, tmp) == 0;
				script_removetop(st, -1, 0);

				if (matches && !neq) {
					return list[i];
				} else if (!matches && neq) {
					return list[i];
				}
			}
		} else if (size >= 1) {
			for (unsigned int i = 0; i < size; i++) {
				if (list[i] < start) {
					continue;
				}

				int64 uid = reference_uid(ref_id, list[i]);
				int tmp = (int)h64BPTRSIZE(script->get_val2(st, uid, ref));
				script_removetop(st, -1, 0);

				if ((int)h64BPTRSIZE(needle) == tmp && !neq) {
					return list[i];
				} else if ((int)h64BPTRSIZE(needle) != tmp && neq) {
					return list[i];
				}
			}
		}
	}

	return -1;
}

static unsigned int script_array_count(struct script_state *st, struct map_session_data *sd, const char *name, uint32 start, struct reg_db *ref, const void *needle, bool neq)
{
	struct reg_db *src = script->array_src(st, sd, name, ref);
	unsigned int count = 0;

	if (src != NULL && src->arrays) {
		int ref_id = script->add_word(name);
		struct script_array *sa = idb_get(src->arrays, ref_id);

		if (sa == NULL) {
			return count;
		}

		if (sa->size >= 1 && is_string_variable(name)) {
			for (unsigned int i = 0; i < sa->size; i++) {
				if (sa->members[i] < start) {
					continue;
				}

				int64 uid = reference_uid(ref_id, sa->members[i]);
				const char *tmp = script->get_val2(st, uid, ref);
				bool matches = strcmp((const char *)needle, tmp) == 0;
				script_removetop(st, -1, 0);

				if (matches && !neq) {
					++count;
				} else if (!matches && neq) {
					++count;
				}
			}
		} else if (sa->size >= 1) {
			for (unsigned int i = 0; i < sa->size; i++) {
				if (sa->members[i] < start) {
					continue;
				}

				int64 uid = reference_uid(ref_id, sa->members[i]);
				int tmp = (int)h64BPTRSIZE(script->get_val2(st, uid, ref));
				script_removetop(st, -1, 0);

				if ((int)h64BPTRSIZE(needle) == tmp && !neq) {
					++count;
				} else if ((int)h64BPTRSIZE(needle) != tmp && neq) {
					++count;
				}
			}
		}
	}

	return count;
}

static unsigned int script_array_replace(struct script_state *st, struct map_session_data *sd, const char *name, uint32 start, struct reg_db *ref, const void *needle, const void *replace, bool neq)
{
	struct reg_db *src = script->array_src(st, sd, name, ref);
	unsigned int count = 0;

	if (src != NULL && src->arrays) {
		int ref_id = script->add_word(name);
		struct script_array *sa = idb_get(src->arrays, ref_id);

		if (sa == NULL) {
			return count;
		}

		if (sa->size >= 1 && is_string_variable(name)) {
			for (unsigned int i = 0; i < sa->size; i++) {
				if (sa->members[i] < start) {
					continue;
				}

				int64 uid = reference_uid(ref_id, sa->members[i]);
				const char *tmp = script->get_val2(st, uid, ref);
				bool matches = strcmp((const char *)needle, tmp) == 0;
				script_removetop(st, -1, 0);

				if ((matches && !neq) || (!matches && neq)) {
					script->set_reg(st, sd, uid, name, replace, ref);
					++count;
				}
			}
		} else if (sa->size >= 1) {
			for (unsigned int i = 0; i < sa->size; i++) {
				if (sa->members[i] < start) {
					continue;
				}

				int64 uid = reference_uid(ref_id, sa->members[i]);
				int tmp = (int)h64BPTRSIZE(script->get_val2(st, uid, ref));
				bool matches = (int)h64BPTRSIZE(needle) == tmp;
				script_removetop(st, -1, 0);

				if ((matches && !neq) || (!matches && neq)) {
					script->set_reg(st, sd, uid, name, replace, ref);
					++count;
				}
			}
		}
	}

	return count;
}

static void *script_array_pop(struct script_state *st, struct map_session_data *sd, const char *name, uint32 start, struct reg_db *ref, bool lifo)
{
	struct reg_db *src = script->array_src(st, sd, name, ref);
	void *out = is_string_variable(name) ? aStrdup("") : (void *)0;

	if (src != NULL && src->arrays) {
		int ref_id = script->add_word(name);
		struct script_array *sa = idb_get(src->arrays, ref_id);

		if (sa == NULL) {
			return out;
		}

		unsigned int *list = sa->members;
		unsigned int size = sa->size;

		if (!lifo) {
			list = script->array_cpy_list(sa);
			qsort(list, size, sizeof(unsigned int), script_array_index_cmp);
		}

		if (size >= 1 && is_string_variable(name) && (lifo || (!lifo && list[size - 1] >= start))) {
			int64 uid = reference_uid(ref_id, list[size - 1]);
			aFree(out); // free the placeholder value
			out = aStrdup(script->get_val2(st, uid, ref));
			script_removetop(st, -1, 0);

			script->set_reg(st, sd, uid, name, "", ref);
		} else if (size >= 1 && (lifo && (!lifo && list[size - 1] >= start))) {
			int64 uid = reference_uid(ref_id, list[size - 1]);
			out = (void *)h64BPTRSIZE(script->get_val2(st, uid, ref));
			script_removetop(st, -1, 0);

			script->set_reg(st, sd, uid, name, (const void *)0, ref);
		}
	}

	return out;
}

static void *script_array_shift(struct script_state *st, struct map_session_data *sd, const char *name, uint32 start, struct reg_db *ref, bool fifo)
{
	struct reg_db *src = script->array_src(st, sd, name, ref);
	void *out = is_string_variable(name) ? aStrdup("") : (void *)0;

	if (src != NULL && src->arrays) {
		int ref_id = script->add_word(name);
		struct script_array *sa = idb_get(src->arrays, ref_id);

		if (sa == NULL) {
			return out;
		}

		unsigned int *list = sa->members;
		unsigned int size = sa->size;
		unsigned int i = 0;

		list = script->array_cpy_list(sa);
		qsort(list, size, sizeof(unsigned int), script_array_index_cmp);

		if (fifo) {
			ARR_FIND(0, size, i, list[i] == sa->members[0]);
		} else {
			ARR_FIND(0, size, i, list[i] == start);
		}

		if (size >= 1 && is_string_variable(name) && (fifo || (!fifo && i < size))) {
			int64 uid = reference_uid(ref_id, list[i]);
			aFree(out); // free the placeholder value
			out = aStrdup(script->get_val2(st, uid, ref));
			script_removetop(st, -1, 0);

			for (i++; i < size; ++i) {
				int64 uid_prev = reference_uid(ref_id, list[i - 1]);
				uid = reference_uid(ref_id, list[i]);
				script->set_reg(st, sd, uid_prev, name, script->get_val2(st, uid, ref), ref);
				script_removetop(st, -1, 0);
			}

			script->set_reg(st, sd, uid, name, "", ref);
		} else if (size >= 1 && (fifo || (!fifo && i < size))) {
			int64 uid = reference_uid(ref_id, list[i]);
			out = (void *)h64BPTRSIZE(script->get_val2(st, uid, ref));
			script_removetop(st, -1, 0);

			for (i++; i < size; ++i) {
				int64 uid_prev = reference_uid(ref_id, list[i - 1]);
				uid = reference_uid(ref_id, list[i]);
				script->set_reg(st, sd, uid_prev, name, script->get_val2(st, uid, ref), ref);
				script_removetop(st, -1, 0);
			}

			script->set_reg(st, sd, uid, name, (const void *)0, ref);
		}
	}

	return out;
}

static unsigned int script_array_remove(struct script_state *st, struct map_session_data *sd, const char *name, uint32 start, struct reg_db *ref, const void *needle, bool neq)
{
	struct reg_db *src = script->array_src(st, sd, name, ref);
	unsigned int count = 0;

	if (src != NULL && src->arrays) {
		int ref_id = script->add_word(name);
		struct script_array *sa = idb_get(src->arrays, ref_id);

		if (sa == NULL) {
			return count;
		}

		unsigned int *list = script->array_cpy_list(sa);
		unsigned int size = sa->size;
		qsort(list, size, sizeof(unsigned int), script_array_index_cmp);

		if (size >= 1 && is_string_variable(name)) {
			for (unsigned int i = 0; i < size; i++) {
				if (list[i] < start) {
					continue;
				}

				int64 uid = reference_uid(ref_id, list[i]);
				const char *tmp = script->get_val2(st, uid, ref);
				bool matches = strcmp((const char *)needle, tmp) == 0;
				script_removetop(st, -1, 0);

				if ((matches && !neq) || (!matches && neq)) {
					for (unsigned int k = i + 1; k < size; ++k) {
						int64 uid_prev = reference_uid(ref_id, list[k - 1]);
						uid = reference_uid(ref_id, list[k]);
						script->set_reg(st, sd, uid_prev, name, script->get_val2(st, uid, ref), ref);
						script_removetop(st, -1, 0);
					}

					script->set_reg(st, sd, uid, name, "", ref);
					++count;
					--i;
				}
			}
		} else if (size >= 1) {
			for (unsigned int i = 0; i < size; i++) {
				if (list[i] < start) {
					continue;
				}

				int64 uid = reference_uid(ref_id, list[i]);
				int tmp = (int)h64BPTRSIZE(script->get_val2(st, uid, ref));
				bool matches = (int)h64BPTRSIZE(needle) == tmp;
				script_removetop(st, -1, 0);

				if ((matches && !neq) || (!matches && neq)) {
					for (unsigned int k = i + 1; k < size; ++k) {
						int64 uid_prev = reference_uid(ref_id, list[k - 1]);
						uid = reference_uid(ref_id, list[k]);
						script->set_reg(st, sd, uid_prev, name, script->get_val2(st, uid, ref), ref);
						script_removetop(st, -1, 0);
					}

					script->set_reg(st, sd, uid, name, 0, ref);
					++count;
					--i;
				}
			}
		}
	}

	return count;
}

// this should be done in script.c
/*
@@ -3436,6 +3771,18 @@ static int set_reg(struct script_state *st, struct map_session_data *sd, int64 n
 		return 0;
 	}

+	if (script->str_data[script_getvarid(num)].type != C_PARAM && script_getvaridx(num) > 0) {
+		struct reg_db *src = script->array_src(st, sd, name, ref);
+		if (src != NULL && src->arrays) {
+			struct script_array *sa = idb_get(src->arrays, script_getvarid(num));
+
+			if (sa == NULL || sa->size < 2) {
+				// make sure index 0 is the first in sa->members
+				script->array_ensure_zero(st, sd, num, ref);
+			}
+		}
+	}
+
 	if (is_string_variable(name)) {// string variable
 		const char *str = (const char*)value;
*/


// the name of this macro could be better...
#define ARRAY_BUILDIN_CHECK() \
	struct script_data* data = script_getdata(st, 2); \
	if (!data_isreference(data) || !reference_tovariable(data)) { \
		ShowError("script:%s: not an array!\n", get_buildin_name(st)); \
		script->reportdata(data); \
		script_pushnil(st); \
		st->state = END; \
		return false; \
	} \
	const char *name = reference_getname(data); \
	uint32 index = reference_getindex(data); \
	struct reg_db *ref = reference_getref(data); \
	struct map_session_data *sd = st->rid ? script->rid2sd(st) : NULL;

static BUILDIN(array_entries)
{
	ARRAY_BUILDIN_CHECK();
	struct reg_db *src = script->array_src(st, sd, name, ref);

	if (src && src->arrays) {
		struct script_array *sa = idb_get(src->arrays, script->search_str(name));

		if (sa && sa->size > 0) {
			unsigned int *list = script->array_cpy_list(sa);
			unsigned int size = sa->size;
			qsort(list, size, sizeof(unsigned int), script_array_index_cmp);

			if (index > list[size - 1]) {
				script_pushint(st, 0);
			} else {
				unsigned int i = 0;
				ARR_FIND(0, size - 1, i, list[i] == index);
				script_pushint(st, size - i);
			}
			return true;
		}
	}

	script_pushint(st, 0);
	return true;
}

static BUILDIN(array_find)
{
	ARRAY_BUILDIN_CHECK();
	bool neq = script_hasdata(st, 4) && script_getnum(st, 4);
	bool reverse = strncmp(get_buildin_name(st), "array_r", 7) == 0;

	if (is_string_variable(name)) {
		const char *needle = script_getstr(st, 3);
		script_pushint(st, script->array_find(st, sd, name, index, ref, needle, reverse, neq));
	} else {
		int needle = script_getnum(st, 3);
		script_pushint(st, script->array_find(st, sd, name, index, ref, (const void *)h64BPTRSIZE(needle), reverse, neq));
	}

	return true;
}

static BUILDIN(array_count)
{
	ARRAY_BUILDIN_CHECK();
	bool neq = script_hasdata(st, 4) && script_getnum(st, 4);

	if (is_string_variable(name)) {
		const char *needle = script_getstr(st, 3);
		script_pushint(st, script->array_count(st, sd, name, index, ref, needle, neq));
	} else {
		int needle = script_getnum(st, 3);
		script_pushint(st, script->array_count(st, sd, name, index, ref, (const void *)h64BPTRSIZE(needle), neq));
	}

	return true;
}

static BUILDIN(array_replace)
{
	ARRAY_BUILDIN_CHECK();
	bool neq = script_hasdata(st, 5) && script_getnum(st, 5);

	if (is_string_variable(name)) {
		const char *needle = script_getstr(st, 3);
		const char *replace = script_getstr(st, 4);
		script_pushint(st, script->array_replace(st, sd, name, index, ref, needle, replace, neq));
	} else {
		const void *needle = (const void *)h64BPTRSIZE(script_getnum(st, 3));
		const void *replace = (const void *)h64BPTRSIZE(script_getnum(st, 4));
		script_pushint(st, script->array_replace(st, sd, name, index, ref, needle, replace, neq));
	}

	return true;
}

static BUILDIN(array_pop)
{
	ARRAY_BUILDIN_CHECK();
	bool lifo = strncmp(get_buildin_name(st), "array_l", 7) == 0;

	if (is_string_variable(name)) {
		script_pushstr(st, (char *)script->array_pop(st, sd, name, index, ref, lifo));
	} else {
		script_pushint(st, (int)h64BPTRSIZE(script->array_pop(st, sd, name, index, ref, lifo)));
	}
	return true;
}

static BUILDIN(array_shift)
{
	ARRAY_BUILDIN_CHECK();
	bool fifo = strncmp(get_buildin_name(st), "array_f", 7) == 0;

	if (is_string_variable(name)) {
		script_pushstr(st, (char *)script->array_shift(st, sd, name, index, ref, fifo));
	} else {
		script_pushint(st, (int)h64BPTRSIZE(script->array_shift(st, sd, name, index, ref, fifo)));
	}
	return true;
}

static BUILDIN(array_remove)
{
	ARRAY_BUILDIN_CHECK();
	bool neq = script_hasdata(st, 4) && script_getnum(st, 4);

	if (is_string_variable(name)) {
		const char *needle = script_getstr(st, 3);
		script_pushint(st, script->array_remove(st, sd, name, index, ref, needle, neq));
	} else {
		const void *needle = (const void *)h64BPTRSIZE(script_getnum(st, 3));
		script_pushint(st, script->array_remove(st, sd, name, index, ref, needle, neq));
	}

	return true;
}

#undef ARRAY_BUILDIN_CHECK


HPExport void plugin_init (void)
{
	if (SERVER_TYPE == SERVER_TYPE_MAP) {
		//addHookPre(script, set_reg, escript_set_reg); <= too dangerous to override

		addScriptCommand("array_entries", "r", array_entries);
		addScriptCommand("array_find", "rv?", array_find);
		addScriptCommand("array_rfind", "rv?", array_find);
		addScriptCommand("array_count", "rv?", array_count);
		addScriptCommand("array_replace", "rvv?", account_replace);
		addScriptCommand("array_pop", "r", array_pop);
		//addScriptCommand("array_lifo_pop", "r", array_pop); <= the override is necessary
		addScriptCommand("array_shift", "r", array_shift);
		//addScriptCommand("array_fifo_shift", "r", array_shift); <= the override is necessary
		addScriptCommand("array_remove", "rv?", array_remove);
	}
}
