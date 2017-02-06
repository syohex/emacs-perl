/*
  Copyright (C) 2017 by Syohei YOSHIDA

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <EXTERN.h>
#include <perl.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <emacs-module.h>

int plugin_is_GPL_compatible;

static char*
retrieve_string(emacs_env *env, emacs_value str, ptrdiff_t *size)
{
	*size = 0;

	env->copy_string_contents(env, str, NULL, size);
	char *p = malloc(*size);
	if (p == NULL) {
		*size = 0;
		return NULL;
	}
	env->copy_string_contents(env, str, p, size);

	return p;
}

static emacs_value
perl_to_elisp(emacs_env *env, PerlInterpreter *my_perl, SV *v)
{
	switch (SvTYPE(v)) {
	case SVt_NULL:
		return env->intern(env, "nil");
	case SVt_IV: {
		if (SvROK(v)) {
			return perl_to_elisp(env, my_perl, SvRV(v));
		}

		IV i = SvIV(v);
		return env->make_integer(env, i);
	}
	case SVt_NV: {
		NV n = SvNV(v);
		return env->make_float(env, n);
	}
	case SVt_PV: {
		STRLEN len;
		char *str = SvPV(v, len);
		return env->make_string(env, str, len);
	}
	case SVt_PVAV: {
		AV *av = (AV*)v;
		ssize_t len = av_len(av) + 1;
		emacs_value *vals = malloc(sizeof(emacs_value) * len);

		for (ssize_t i = 0; i < len; ++i) {
			SV **elm = av_fetch(av, i, FALSE);
			vals[i] = perl_to_elisp(env, my_perl, *elm);
		}

		emacs_value Qlist = env->intern(env, "list");
		return env->funcall(env, Qlist, len, vals);
	}
	case SVt_PVHV: {
		emacs_value Fmake_hash_table = env->intern(env, "make-hash-table");
		emacs_value Qtest = env->intern(env, ":test");
		emacs_value Fequal = env->intern(env, "equal");
		emacs_value make_hash_args[] = {Qtest, Fequal};
		emacs_value hash = env->funcall(env, Fmake_hash_table, 2, make_hash_args);
		emacs_value Fputhash = env->intern(env, "puthash");

		HV *hv = (HV*)v;
		HE *he;
		hv_iterinit(hv);
		while((he = hv_iternext(hv)) != NULL){
			STRLEN len;
			SV *k = hv_iterkeysv(he);
			char *kk = SvPV(k, len);
			SV **v = hv_fetch(hv, kk, len, 0);

			emacs_value key = perl_to_elisp(env, my_perl, k);
			emacs_value val = perl_to_elisp(env, my_perl, *v);

			emacs_value puthash_args[] = {key, val, hash};
			env->funcall(env, Fputhash, 3, puthash_args);
		}

		return hash;
	}
	case SVt_PVIV:
	case SVt_PVNV:
	case SVt_PVMG:
	case SVt_INVLIST:
	case SVt_REGEXP:
	case SVt_PVGV:
	case SVt_PVLV:
	case SVt_PVCV:
	case SVt_PVFM:
	case SVt_PVIO:
	case SVt_LAST:
	default:
		fprintf(stderr, "@@ type %d is not supported yet\n", SvTYPE(v));
		break;
	}

	return env->intern(env, "nil");
}

static void
myperl_free(void *arg)
{
	PerlInterpreter *my_perl = (PerlInterpreter*)arg;
	perl_destruct(my_perl);
	perl_free(my_perl);
}

static emacs_value
Fperl_init(emacs_env *env, ptrdiff_t nargs, emacs_value args[], void *data)
{
	char *embedding[] = { "", "-e", "0" };

	PerlInterpreter *my_perl = perl_alloc();
	PERL_SET_CONTEXT(my_perl);
	perl_construct(my_perl);
	perl_parse(my_perl, NULL, 3, embedding, NULL);
	perl_run(my_perl);
	return env->make_user_ptr(env, myperl_free, my_perl);
}

static void
my_eval_pv(PerlInterpreter *my_perl, SV *code)
{
	dSP;

	PUSHMARK(SP);
	eval_sv(code, G_SCALAR);

	SPAGAIN;
	//retval = POPs;
	PUTBACK;
}

static emacs_value
Fperl_do(emacs_env *env, ptrdiff_t nargs, emacs_value args[], void *data)
{
	PerlInterpreter *my_perl = env->get_user_ptr(env, args[0]);
	ptrdiff_t size;
	char *code = retrieve_string(env, args[1], &size);
	PERL_SET_CONTEXT(my_perl);

	//eval_pv(code, FALSE);

	SV *command = newSV(0);
	sv_setpv(command, code);

	my_eval_pv(my_perl, command);
	SvREFCNT_dec(command);

	free(code);
	return env->intern(env, "t");
}

static emacs_value
Fperl_get_sv(emacs_env *env, ptrdiff_t nargs, emacs_value args[], void *data)
{
	PerlInterpreter *my_perl = env->get_user_ptr(env, args[0]);
	ptrdiff_t size;
	char *varname = retrieve_string(env, args[1], &size);
	PERL_SET_CONTEXT(my_perl);

	SV *v = get_sv(varname, 0);
	free(varname);

	emacs_value retval = perl_to_elisp(env, my_perl, v);
	return retval;
}

static emacs_value
Fperl_get_av(emacs_env *env, ptrdiff_t nargs, emacs_value args[], void *data)
{
	PerlInterpreter *my_perl = env->get_user_ptr(env, args[0]);
	ptrdiff_t size;
	char *varname = retrieve_string(env, args[1], &size);
	PERL_SET_CONTEXT(my_perl);

	AV *v = get_av(varname, 0);
	free(varname);

	return perl_to_elisp(env, my_perl, (SV*)v);
}

static emacs_value
Fperl_get_hv(emacs_env *env, ptrdiff_t nargs, emacs_value args[], void *data)
{
	PerlInterpreter *my_perl = env->get_user_ptr(env, args[0]);
	ptrdiff_t size;
	char *varname = retrieve_string(env, args[1], &size);
	PERL_SET_CONTEXT(my_perl);

	HV *v = get_hv(varname, 0);
	free(varname);

	return perl_to_elisp(env, my_perl, (SV*)v);
}

static void
bind_function(emacs_env *env, const char *name, emacs_value Sfun)
{
	emacs_value Qfset = env->intern(env, "fset");
	emacs_value Qsym = env->intern(env, name);
	emacs_value args[] = { Qsym, Sfun };

	env->funcall(env, Qfset, 2, args);
}

static void
provide(emacs_env *env, const char *feature)
{
	emacs_value Qfeat = env->intern(env, feature);
	emacs_value Qprovide = env->intern (env, "provide");
	emacs_value args[] = { Qfeat };

	env->funcall(env, Qprovide, 1, args);
}

int
emacs_module_init(struct emacs_runtime *ert)
{
	emacs_env *env = ert->get_environment(ert);

#define DEFUN(lsym, csym, amin, amax, doc, data) \
	bind_function (env, lsym, env->make_function(env, amin, amax, csym, doc, data))

	DEFUN("perl-core-init", Fperl_init, 0, 0, "Initialize perl state", NULL);
	DEFUN("perl-core-do", Fperl_do, 2, 2, "Do perl code", NULL);
	DEFUN("perl-core-get-sv", Fperl_get_sv, 2, 2, "Get scalar value", NULL);
	DEFUN("perl-core-get-av", Fperl_get_av, 2, 2, "Get array value", NULL);
	DEFUN("perl-core-get-hv", Fperl_get_hv, 2, 2, "Get hash value", NULL);

#undef DEFUN

	provide(env, "perl-core");
	return 0;
}

/*
  Local Variables:
  c-basic-offset: 8
  indent-tabs-mode: t
  End:
*/
