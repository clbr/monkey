#include <monkey.h>

// All defaults, start, stop

int main() {

	mklib_ctx c = mklib_init(NULL, 0, 0, NULL, NULL, NULL, NULL, NULL);
	if (!c) return 1;

	if (!mklib_start(c)) return 1;
	if (!mklib_stop(c)) return 1;

	return 0;
}