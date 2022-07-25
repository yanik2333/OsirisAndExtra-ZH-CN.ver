#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"

#include "Tickbase.h"

#include "../SDK/Entity.h"
#include "../SDK/UserCmd.h"
#include "../SDK/NetworkChannel.h"

int targetTickShift{ 0 };
int tickShift{ 0 };
int shiftCommand{ 0 };
int shiftedTickbase{ 0 };
int ticksAllowedForProcessing{ 0 };
int chokedPackets{ 0 };
float realTime{ 0.0f };

void Tickbase::run(UserCmd* cmd, bool sendPacket) noexcept
{
    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (const auto netChannel = interfaces->engine->getNetworkChannel(); netChannel)
    {
        if (netChannel->chokedPackets > chokedPackets)
            chokedPackets = netChannel->chokedPackets;
        if (netChannel->chokedPackets == 0)
            ticksAllowedForProcessing = max(getMaxUserCmdProcessTicks() - chokedPackets, 0);
    }
}

void Tickbase::shift(UserCmd* cmd, int shiftAmount) noexcept
{
    if (!canFire(shiftAmount))
        return;

    realTime = memory->globalVars->realtime;
    shiftedTickbase = shiftAmount;
    shiftCommand = cmd->commandNumber;
    tickShift = shiftAmount;
}

bool Tickbase::canRun() noexcept
{
    if (!interfaces->engine->isInGame() || !interfaces->engine->isConnected())
        return true;

    if (!localPlayer || !localPlayer->isAlive() || !targetTickShift)
    {
        ticksAllowedForProcessing = 0;
        chokedPackets = 0;
        return true;
    }

    if (config->misc.fakeduck && config->misc.fakeduckKey.isActive())
    {
        realTime = memory->globalVars->realtime;
        return true;
    }

    if (ticksAllowedForProcessing < targetTickShift && memory->globalVars->realtime - realTime > 1.0f)
    {
        ticksAllowedForProcessing++;
        chokedPackets--;
        return false;
    }
    return true;
}

bool Tickbase::canFire(int shiftAmount) noexcept
{
    if (!localPlayer || !localPlayer->isAlive())
        return false;

    if (shiftAmount > ticksAllowedForProcessing)
        return false;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon || !activeWeapon->clip())
        return false;

    if (activeWeapon->isKnife() || activeWeapon->isGrenade() || activeWeapon->isBomb()
        || activeWeapon->itemDefinitionIndex2() == WeaponId::Revolver
        || activeWeapon->itemDefinitionIndex2() == WeaponId::Taser)
        return false;

    const float shiftTime = (localPlayer->tickBase() - shiftAmount) * memory->globalVars->intervalPerTick;
    if (localPlayer->nextAttack() > shiftTime)
        return false;

	return activeWeapon->nextPrimaryAttack() <= shiftTime;
}

int Tickbase::getCorrectTickbase(int commandNumber) noexcept
{
	const int tickBase = localPlayer->tickBase();

	if (commandNumber == shiftCommand)
		return tickBase - shiftedTickbase;
	else if (commandNumber == shiftCommand + 1)
		return tickBase + shiftedTickbase;

	return tickBase;
}

//If you have dt enabled, you need to shift 13 ticks, so it will return 13 ticks
//If you have hs enabled, you need to shift 9 ticks, so it will return 7 ticks
int Tickbase::getTargetTickShift() noexcept
{
	return targetTickShift;
}

int Tickbase::getTickshift() noexcept
{
	return tickShift;
}

void Tickbase::resetTickshift() noexcept
{
	shiftedTickbase = tickShift;
    ticksAllowedForProcessing = max(ticksAllowedForProcessing - tickShift, 0);
	tickShift = 0;
}

void Tickbase::reset() noexcept
{
    chokedPackets = 0;
    tickShift = 0;
    shiftCommand = 0;
    shiftedTickbase = 0;
    ticksAllowedForProcessing = 0;
    realTime = 0.0f;
}