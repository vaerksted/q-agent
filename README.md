## Quintuple Agent

The code was upgraded for autoconf 2.69 and gettext 0.19.8.1.

Automake 1.15 output suffix rules warnings on Makefile.am files.

```
bash-4.3# automake
Makefile.am:50: warning: '%'-style pattern rules are a GNU make extension
doc/Makefile.am:18: warning: '%'-style pattern rules are a GNU make extension
doc/Makefile.am:21: warning: '%'-style pattern rules are a GNU make extension
doc/Makefile.am:30: warning: '%'-style pattern rules are a GNU make extension
bash-4.3#
```

Correct the .texi file formatting.

```
bash-4.3# sed -i 's/{Q/ Q/; s/nt}/nt/; s/{R/ R/; s/r}/r/' doc/manual.texi
bash-4.3# 
```

Running `make install` doesn't install an .info page.

The malloc patch hasn't been applied.

If you experience problems running `agpg`.

```
bash-4.3$ cat quintuple-agent_1.0.4-6.diff \                                  
> | perl -lae 'print if $. == 953..973'
+--- quintuple-agent-1.0.4.orig/agpg.c	2002-09-28 07:16:01.000000000 +0000
++++ quintuple-agent-1.0.4/agpg.c	2005-02-21 21:49:21.466050839 +0000
+@@ -100,11 +100,13 @@
+   if (id)
+     free(buf);
+   while ((len = getline(&line, &size, gpg)) > 0) {
+-    if (len > 10 && !strncmp(line, "sec ", 4) && line[10] == '/') {
+-      char *x;
+-      if ((x = strchr(line + 11, ' ')) != NULL) {
+-	*x = 0;
+-	id = strdup(line + 11);
++#define GPG_SECKEYS_DELIM " \t/"
++    if (strncmp(line, "sec ", 4) == 0 &&
++      strtok(line, GPG_SECKEYS_DELIM) &&
++      strtok(NULL, GPG_SECKEYS_DELIM)) {
++    char *x;
++  if ((x = strtok(NULL, GPG_SECKEYS_DELIM)) != NULL) {
++    id = strdup(x);
+ 	free(line);
+ 	pclose(gpg);
+ 	return id;
bash-4.3$ 
```

Setting `GTK_RC_FILES` to one of the files in `/etc/gtk` eliminates the error messages when running the GUIs.