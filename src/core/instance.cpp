#include "core/instance.hpp"
#include "core/settings.hpp"
#include "core/command.hpp"

emulator_instance::emulator_instance()
	: setcache(lsnes_vset), subtitles(&mlogic), mbranch(&mlogic), mteditor(&mlogic),
	status(status_A, status_B, status_C), mapper(keyboard, lsnes_cmd)
{
	status_A.valid = false;
	status_B.valid = false;
	status_C.valid = false;
}

emulator_instance lsnes_instance;

emulator_instance& CORE()
{
	return lsnes_instance;
}