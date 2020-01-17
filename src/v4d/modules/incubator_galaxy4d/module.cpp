// Metadata
#define _V4D_MODULE
#define THIS_MODULE_VENDOR_ID 16
#define THIS_MODULE_ID 1
#define THIS_MODULE_REV 1
#define THIS_MODULE_NAME "Galaxy4D"
#define THIS_MODULE_DESCRIPTION "Full-scale space sim"

// V4D Core Header
#include <v4d.h>
#include "Planets.hpp"

V4DMODULE void V4D_ModuleCreate() {
	V4D_LOAD_SUBMODULE(Planets)
}

V4DMODULE void V4D_ModuleDestroy() {
	V4D_UNLOAD_SUBMODULES(Planets)
}
