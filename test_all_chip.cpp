#include "okecc.h"

void test_sub1(){
	startSub(1);
	
	If(isLineBlocked && isLineBlockedByTerrain) nop; End
	If(isLineBlockedByFriendly && isLineBlockedByBarrier) nop; End
	
	If(A >= B && C <= D && E == F) nop; End
	If(G == -99999.99 && H != 99999) nop; End
	
	If(isButton(BT_UP | BT_L | BT_PUSH_ALL) && isButton(BT_UP | BT_R | BT_CHANGE)) nop; End
	
	If(analogX >= -128 && analogY <= 127) nop; End
	
	stop;
	stopMove;
	stopTurn;
	stopFire;

	moveForward;
	moveBackward;
	moveLeft;
	moveRight;
	moveForwardLeft;
	moveBackwardRight.dist(500).wait;
	
	moveForward.fast;
	moveBackward.fast;
	moveLeft.fast.wait;
	moveRight.fast.wait;
	
	
	turnLeft;
	turnRight.angle(360).wait;
	turnLeft.fast;
	turnRight.fast.wait;
	
	jumpForward;
	jumpBackward;
	jumpLeft.wait;
	jumpRight.wait;
	jumpUp;
	
	fightLow;
	fightHigh;
	fightLong;
	fight.wait;
}

void test_sub2(){
	startSub(2);
	
	guard(5);
	crouch(60).wait;
	
	special(1);
	special(3).wait;
	
	fire(W_ACTIVE, 16).h(0);
	fire(1, 1).h(0).wide;
	fire(5, 1).snipe.wait;
	
	fire(1, 16).target;
	fire(5, 1).target.snipe.wait;
	
	fire(0, 90, 1, 16);
	fire(180, -90, 5, 1).wait;
	
	option(1);
	option(5);
	
	lockon;
	lockonFriendly.type(OKE_BIPED).farthest;
	lockonAll.type(OKE_GROUND).front;
	
	autoTurn.h(0);
	autoTurn.span(0);
	autoTurn.off;
	
	lockonPart(BODY);
	lockonPart(1);
	lockonPart(5);
	
	lockonId(A);
	lockonId(H);
	
	ascend;
	descend;
	setAltitude(20);
	setAltitude(100);

	unlock;
	aimGunsight(180, 90);
	aimGunsight(-179, -90).rel;
	aimGunsight(A, H);
	aimGunsight(H, A).rel;

	gunsight(1);
	gunsight(5);
	gunsight(GS_OFF);
	gunsight(GS_NEXT);
	
	viewBack;
	viewFront;
	viewLeft;
	viewRight;
	viewTop;
	viewGunsight;
	viewPair.far;
	viewNext.near;
	
	alert(AL_NOSOUND, 1, AL_RIGHT);
	alert(1, 5, AL_NONE);
	alert(5, 1, AL_CENTER);
}

void chip_main(){
	nop;
	sleep(1);
	sleep(120);
	wait;
	waitMove;
	waitTurn;
	waitFire;
	
	A = numAllEnemy;
	H = numAllFriendly;
	A = time;
	A = mathRand;
	A = myX;
	A = myY;
	A = myZ;
	A = myDirection;
	A = targetId;
	A = targetAzimuth;
	A = targetElevation;
	A = targetX;
	A = targetY;
	A = targetZ;
	A = targetDirection;
	A = targetBodyCode;
	A = targetActCode;
	A = targetDistance;
	A = targetDistanceXy;

	A = timeRemained;
	A = myId;
	A = myActCode;
	A = heat;
	A = health;
	A = energy;
	A = mySpeed;
	A = targetSpeed;
	A = numAmmo(5);
	A = numOption(4);
	A = raderRange;
	A = mathPi;
	A = buttonValue;
	A = analogX;
	A = analogY;
	A = gunsightDirection;
	A = gunsightElevation;
	A = gunsightNum;

	A = H;
	H = A;
	A += B;
	B -= C;
	C *= D;
	D /= E;
	E %= F;
	E &= F;
	E |= F;
	E ^= F;
	A = 0;
	B = 99999.99;
	C = -99999.99;
	
	A = mathInt(99999.99);
	B = mathAbs(-1);
	C = mathMax(1);
	A = mathMin(H);
	H = mathSqr(A);
	H = mathSin(A);
	H = mathCos(A);
	H = mathTan(A);
	H = mathAtan(A);
	H = mathNot(A);
	
	getButton;
	getButton.wait;
	
	chSend(A, 8);
	chSend(H, 1);
	A = chReceive(8);
	H = chReceive(1);
	
	If (numAmmo(0) >= 1 && numAmmo(5) > 990) nop; End
	If (numOption(1) <= 1 || numOption(4) < 1) nop; End

	If(isBarrierOver(1).h(0) && isBarrierOver(1).w(0)) nop; End
	If(isBarrierOver(1).x(800) && isBarrierOver(1).y(800)) nop; End
	If(isBarrierOver(30)) nop; End
	
	If(isBarrierUnder(1).span(0)) nop; End
	If(isBarrierUnder(1).dist(0) || isBarrierUnder(1).minDist(800)) nop; End
	If(isBarrierUnder(30).dir(-180) || isBarrierUnder(30).dir(180)) nop; End

	If(isOutsideArea.h(0) || isOutsideArea.dir(180)) nop; End
	
	If(numEnemy.h(0) >= 1) nop; End
	If(numFriendly.h(100).type(OKE_BIPED) >= 1) nop; End
	If(numAllOke.h(200).type(OKE_QUADRUPED) >= 1) nop; End
	If(numUnknownOke.h(400).type(OKE_HOVER) >= 1) nop; End
	If(numEnemy.h(800).type(OKE_VEHICLE) >= 3) nop; End
	If(numEnemy.dist(0).type(OKE_FLIGHT) <= 3) nop; End
	
	If(
		numProjectile.h(0).type(P_BULLET) >= 1 &&
		numProjectile.dist(0).type(P_BEAM) > 1 &&
		numProjectile.type(P_PULSE) <= 1
	) nop; End
	If(
		numProjectile.type(P_NAPALM) < 1 &&
		numProjectile.type(P_GRENADE) >= 1 &&
		numProjectile.type(P_BOMB) >= 1
	) nop; End
	If(
		numProjectile.type(P_ROCKET) >= 1 &&
		numProjectile.type(P_MISSILE) >= 1 &&
		numProjectile.type(P_MINE) >= 1
	) nop; End
	If(
		numProjectile.type(P_FMINE) >= 1 &&
		numProjectile.type(P_HI_V) >= 3 &&
		numProjectile.type(P_ALL) <= 3
	) nop; End
	
	If(health >= 0 && energy <= 0 && heat <= 100) nop; End
	
	If(isSelfWaiting && isSelfMoving    && isSelfTurning ) nop; End
	If(isSelfJumping && isSelfFiring    && isSelfFighting) nop; End
	If(isSelfSpecial && isSelfStumbling && isLocked		 ) nop; End
	If(isOptionActive(1) && isOptionActive(4)) nop; End
	If(isWeaponActive(1) && isWeaponActive(4)) nop; End
	If(isGunsightActive && isAutoTurnActive) nop; End
	
	If(isTargetStumbling ) nop; End
	If(isTargetSheldActive && isTargetOverheat && isMutualLock) nop; End
	
	If(isRand(1) && isRand(99)) nop; End
	
	If(time >= 1 && time <= 300 && timeRemained <= 300) Return; End
	
	If(isTargetPosition.h(0) && isPositionFromTarget.dist(0)) Return; End

	If(targetBodyCode == BC_UNLOCK && targetBodyCode != OKE_FLIGHT) Return; End
	
	If(numLocked != 1 && numLocked == 3) Return; End
	
	If(targetWeaponId(1) == W_NONE && targetWeaponId(2) == W_ASSAULT && targetWeaponId(3) == W_BEAM) Return; End
	If(targetWeaponId(4) == W_PULSE && targetWeaponId(5) == W_NAPALM && targetWeaponId(1) == W_FLAK) Return; End
	If(targetWeaponId(1) == W_SHOTGUN && targetWeaponId(1) == W_CANNON && targetWeaponId(1) == W_RAILGUN) Return; End
	If(targetWeaponId(1) == W_GRENADE && targetWeaponId(1) == W_BOMB && targetWeaponId(1) == W_ROCKET) Return; End
	If(targetWeaponId(1) == W_MISSILE && targetWeaponId(1) == W_MINE && targetWeaponId(1) == W_FMINE) Return; End
	
	test_sub1();
	test_sub2();
}
