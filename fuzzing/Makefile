all: instrument corpus go

instrument:
	./instrument.sh
	echo "Instrumented..."

corpus:
	./make_corpus.sh
	echo "Corpus created..."

go:
	./fuzz.sh
	echo "working on it"

reset:
	patch -R ../sr_unix/sig_init.c sig_init.c.patch

clean:
	rm -rf build-instrumented inputs env YDBTest
	patch -R ../sr_unix/sig_init.c sig_init.c.patch
