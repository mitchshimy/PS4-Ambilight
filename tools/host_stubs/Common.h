// PC test stand-in for the GoldHEN SDK's <Common.h>. Only what the plugin's
// own headers need to parse: HOOK_EXTERN normally declares a typed hook
// pointer, here it just has to be a valid declaration.
#define HOOK_EXTERN(x) extern char x##_host_stub
// The plugin's debug_printf macros call klog(); on a PC it is a no-op the test defines.
void klog(const char *fmt, ...);
#include <orbis/libkernel.h>
