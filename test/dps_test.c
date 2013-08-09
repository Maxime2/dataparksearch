#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define DPS_TEST_PASS 0
#define DPS_TEST_FAIL 1
#define DPS_TEST_SKIP 2

#define debug(STRING...) _debug(__FILE__, __LINE__, __FUNCTION__, ## STRING)

FILE *log;

static char *str_store (char *dest, char *src) {
	size_t d_size = (dest ? strlen(dest) : 0);
	size_t s_size = strlen(src) + 1;
	char *_ = realloc(dest, d_size + s_size);

	if (_) memcpy(_ + d_size, src, s_size);
	else free(dest);
	return(_);
}

static char *ParseEnvVar (char *str) {
	char *p1 = (char *)str;
	char *p2 = (char *)str;
	char *p3;
	char *s;
	char *_ = NULL;

	if (! str) return(NULL);
	while ((p1 = strchr(p1, '$'))) {
		if (p1[1] != '(') {
			p1++;
			continue;
		}
		*p1 = 0;
		_ = str_store(_, p2);
		*p1 = '$';
		if ((p3 = strchr(p1 + 2, ')'))) {
			*p3 = 0;
			s = (char *)getenv(p1 + 2);
			if (s) _ = str_store(_, s);
			*p3 = ')';
			p2 = p1 = p3 + 1;
		} else {
			free(_);
			return(NULL);
		}
	}
	_ = str_store(_, p2);
	return(_);
}

static char *strf (const char *fmt, va_list ap) {
	int nc;
	char *str = NULL;
	char *p;
	size_t size = 512;

	while (1) {
		p = realloc(str, size);
		if (! p) {
			free(str);
			str = NULL;
			break;
		}
		str = p;
		nc = vsnprintf(str, size, fmt, ap);
		if (nc < size) break;
		size *= 2;
	}
	return(str);
}

static char *mem_printf (const char *fmt, ...) {
	va_list ap;
        char *_;

        va_start(ap, fmt);
        _ = strf(fmt, ap);
        va_end(ap);
        return(_);
}

static void _debug (const char *file, int line, const char *func, const char *fmt, ...) {
	va_list list;
	time_t t = time(NULL);
	char *tim = ctime(&t);
	char *str;

	tim += 4;
	tim[strlen(tim) - 6] = 0;

	va_start(list, fmt);
	str = strf(fmt, list);
	va_end(list);

	fseek(log, 0, SEEK_END);
	fprintf(log, "%s %s(%d) [%s]: %s\n", tim, file, line, func, str);
	fflush(log);
}

static int testenv (char *key, char *val) {
	char *p = getenv(key);
	if (! p) return(1);
	else if (*val && strcmp(p, val)) return(2);
	return(0);
}

static int mdiff (char *src, char *dst) {
	int src_fd;
	int dst_fd;
	char src_buf[1024];
	char dst_buf[sizeof(src_buf)];
	ssize_t src_rs;
	ssize_t dst_rs;
	int _ = 0;

	src_fd = open(src, O_RDONLY);
	if (src_fd < 0) {
		debug("Unable to open src (%s).", strerror(errno));
		return(1);
	}

	dst_fd = open(dst, O_RDONLY);
	if (dst_fd < 0) {
		close(src_fd);
		debug("Unable to open dst (%s).", strerror(errno));
		return(1);
	}

	while (1) {
	    src_rs = read(src_fd, src_buf, sizeof(src_buf));
	    dst_rs = read(dst_fd, dst_buf, sizeof(dst_buf));

	    if (src_rs > dst_rs) {
		    debug("src > dst.");
		    _ = 1;
		    break;
	    } else if (src_rs < dst_rs) {
		    debug("src < dst.");
		    _ = 1;
		    break;
	    } else if (! src_rs) break;

	    if (memcmp(src_buf, dst_buf, src_rs)) {
		    debug("src and dst differs.");
		    _ = 1;
		    break;
	    }
	}
	close(src_fd);
	close(dst_fd);
	return(_);
}

static int DpsTest (char *test) {
	char *p;
	char *p1;
	FILE *fp;
	char buf[4096];
	int _ = DPS_TEST_FAIL;
	int e;
	int r;
	int c;
	int cr;
	char *str;
	size_t l;
	size_t i;

	fp = fopen(test, "r");
	if (! fp) {
		debug("fopen failed: %s", strerror(errno));
		return(DPS_TEST_FAIL);
	}

	while (fgets(buf, sizeof(buf), fp)) {
		char *end;
		if (buf[0]=='#')continue;
		for (end=buf+strlen(buf)-1; (end>=buf) && strchr(" \r\n\t",end[0]); *end--='\0');
		if (!buf[0])continue;
		
		debug("%s", buf);
		str = ParseEnvVar(buf);
		if (! str) {
			debug("ParseEnvVar failed.");
			continue;
		}
		p = str;
		while (isspace(*p)) p++;

		if (! strncmp(p, "pass", 4)) {
			c = DPS_TEST_PASS;
			p += 4;
		} else if (! strncmp(p, "fail", 4)) {
			c = DPS_TEST_FAIL;
			p += 4;
		} else if (! strncmp(p, "skip", 4)) {
			c = DPS_TEST_SKIP;
			p += 4;
		} else {
			debug("Syntax error");
			free(str);
			_ = DPS_TEST_FAIL;
			break;
		}

		while (isspace(*p)) p++;

		if (*p == '!') {
			e = 1;
			p++;
			while (isspace(*p)) p++;
		} else e = 0;

		p1 = p;
		while (*p1 && ! isspace(*p1)) p1++;
		if (! *p1) {
			free(str);
			_ = DPS_TEST_FAIL;
			break;
		}
		*p1 = 0;
		r = atoi(p);
		p = p1 + 1;
		while (isspace(*p)) p++;
		l = strlen(p);
		for (i = l; i > 0; i--) {
			if (isspace(p[l - 1])) p[l - 1] = 0;
			else break;
		}
		if (! strncmp(p, "exec", 4)) {
			p += 4;
			while (isspace(*p)) p++;
			cr = system(p);
		} else if (! strncmp(p, "mdiff", 5)) {
			char *f1, *f2;
			p += 5;
			while (isspace(*p)) p++;
			f1 = p;
			while (*p && ! isspace(*p)) p++;
			*p = 0;
			p++;
			while (isspace(*p)) p++;
			f2 = p;
			cr = mdiff(f1, f2);
		} else if (! strncmp(p, "testenv", 7)) {
			char *f1, *f2;
			p += 8;
			while (isspace(*p)) p++;
			f1 = p;
			while (*p && ! isspace(*p)) p++;
			*p = 0;
			p++;
			while (isspace(*p)) p++;
			f2 = p;
			cr = testenv(f1, f2);
		} else {
			free(str);
			debug("Syntax error.\n");
			_ = DPS_TEST_FAIL;
			break;
		}
		if ((cr == r) ^ e) {
			free(str);
			_ = c;
			debug("Test condition: true");
			break;
		} else {
			debug("Test condition: false");
		}
	}
	fclose(fp);
	return(_);
}

static char **split (const char *sep, char *str) {
	char *p;
	char **_ = NULL;
	char *buf;
	size_t c = 0;

	buf = strdup(str);
	if (! buf) return(NULL);
	p = strtok(buf, sep);
	while (p) {
		c++;
		_ = realloc(_, sizeof(char *) * c);
		_[c - 1] = p;
		p = strtok(NULL, sep);
	}
	c++;
	_ = realloc(_, sizeof(char *) * c);
	_[c - 1] = NULL;
	return(_);
}

static void DpsSetDBAddrs (char *str) {
	char **a;
	char **p;
	char *key;
	size_t c = 0;

	a = split(",", str);
	for (p = a; *p; p++) {
		key = mem_printf("DPS_TEST_DBADDR%d", c);
		setenv(key, *p, 1);
		c++;
		free(key);
	}
	free(a);
}

static void DpsUnSetDBAddrs (char *str) {
	char **a;
	char **p;
	char *key;
	size_t c = 0;

	a = split(",", str);
	for (p = a; *p; p++) {
		key = mem_printf("DPS_TEST_DBADDR%d", c);
		unsetenv(key);
		c++;
		free(key);
	}
	free(a);
}

static void about(void)
{
fprintf(stderr,"\n\
In order to run DataparkSearch's test suit please set \n\
DPS_TEST_DBADDR environment variable with a semicolon \n\
separated list of DBAddr you'd like to run tests with. \n\
\n\
You can do it by adding a line into your ~.bashrc using this example:\n\
\n\
export DPS_TEST_DBADDR=\"mysql://localhost/test/;pgsql://root@/root/\"\n\
\n\
Note, tests will destroy all existing data in the given databases,\n\
please use temporary databases for tests purposes.\n\
\n");
}

int main (int argc, char **argv) {
	DIR *d;
	struct dirent *de;
	struct stat st;
	int i;
	char *p;
	char *DPS_TEST_ROOT;
	char *DPS_TEST_LOG;
	char *DPS_TEST_DBADDR;
	char **DBAddrs;
	char **pdb;
	char *mask;
	size_t test_pass = 0;
	size_t test_fail = 0;
	size_t test_skip = 0;
	int res;
	
	mask= (argc == 2) ? argv[1] : NULL;
	
	if (! (DPS_TEST_DBADDR = getenv("DPS_TEST_DBADDR"))) {
		fprintf(stderr, "\nEnvironment variable 'DPS_TEST_DBADDR' is not set.\n");
		about();
		return(1);
	}

	if (! (DPS_TEST_LOG = getenv("DPS_TEST_LOG"))) {
		fprintf(stderr, "Environment variable 'DPS_TEST_LOG' is not set.\n");
		return(1);
	}

	if (! (DPS_TEST_ROOT = getenv("DPS_TEST_ROOT"))) {
		fprintf(stderr, "Environment variable 'DPS_TEST_ROOT' is not set.\n");
		return(1);
	}

	if (! (d = opendir(DPS_TEST_ROOT))) {
		fprintf(stderr, "opendir '%s' failed (%s).", DPS_TEST_ROOT, strerror(errno));
		return(1);
	}

	if (! (log = fopen(DPS_TEST_LOG, "w"))) {
		fprintf(stderr, "fopen '%s' failed: %s.\n", DPS_TEST_LOG, strerror(errno));
		return(1);
	}

	printf("Starting tests from '%s'.\n", DPS_TEST_ROOT);
	debug("Starting test from '%s'.", DPS_TEST_ROOT);

	DBAddrs = split(";", DPS_TEST_DBADDR);
	for (pdb = DBAddrs; *pdb; pdb++) {
		DpsSetDBAddrs(*pdb);
		printf("DBAddr: %s\n", *pdb);
		debug("DBAddr: %s", *pdb);
		rewinddir(d);
		while ((de = readdir(d))) {
			if (*de->d_name == '.') continue;
			if (strncmp(de->d_name,"test",4)) continue;
			if (mask && strcmp(de->d_name,mask)) continue;
			p = mem_printf("%s/%s", DPS_TEST_ROOT, de->d_name);
			if (! p) {
				debug("mem_printf failed.");
        			continue;
			}
			if (lstat(p, &st) < 0) {
				debug("stat %s failed (%s).", de->d_name, strerror(errno));
				continue;
			}
			if (! S_ISDIR(st.st_mode)) continue;
			debug("Starting test %s",de->d_name);
			printf("%s", de->d_name);
			for (i = 0; i < (int)(35 - strlen(de->d_name)); i++) putchar('.');
			if (setenv("DPS_ETC_DIR", p, 1) < 0) {
				debug("setenv failed (%s).", strerror(errno));
				free(p);
				continue;
			}
			if (setenv("DPS_TEST_DIR", p, 1) < 0) {
				debug("setenv failed (%s).", strerror(errno));
				free(p);
				continue;
			}
			free(p);
			p = mem_printf("%s/%s/test.cmd", DPS_TEST_ROOT, de->d_name);
			if (! p) {
				debug("mem_printf failed.");
				continue;
			}
			debug("Running: %s", p);
			res = DpsTest(p);
			free(p);
			switch (res) {
				case DPS_TEST_PASS: printf("passed.\n"); test_pass++; break;
				case DPS_TEST_FAIL: printf("failed.\n"); test_fail++; break;
				case DPS_TEST_SKIP: printf("skipped.\n"); test_skip++; break;
			}
			debug("----------------------------------");
			debug("------------- END OF %s",de->d_name);
			debug("----------------------------------");
		}
		DpsUnSetDBAddrs(*pdb);
	}

	printf("Statistic: total %d, passed %d, skipped %d, failed %d.\n", test_pass + test_fail + test_skip, test_pass, test_skip, test_fail);
	if (test_fail) {
		printf("Some tests failed. Please check '%s' for further detailes.\n", DPS_TEST_LOG);
	}
	return test_fail ? 1 : 0;
}
