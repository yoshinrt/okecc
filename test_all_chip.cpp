#include "okecc.h"

void test_sub1(){
	startSub(1);
	
	stop;
	
	moveForward;
	moveBackward;
	moveLeft;
	moveRight;
	
	moveForward.fast;
	moveBackward.fast;
	moveLeft.fast.wait;
	moveRight.fast.wait;
	
	moveForward.turnRight;
	moveBackward.turnLeft;
	moveLeft.turnRight;
	moveRight.turnLeft;
	
	turnLeft;
	turnRight;
	
	jumpForward;
	jumpBackward;
	jumpLeft.wait;
	jumpRight.wait;
	
	jumpForward.turnLeft;
	jumpBackward.turnRight;
	jumpLeft.turnLeft.wait;
	jumpRight.turnRight.wait;
	
	turnLeft.fast;
	turnRight.fast.wait;
	
	fightLow;
	fightHigh;
	fightLong;
	fight.wait;
	
	guard(5);
	crouch(60).wait;
	
	special(1);
	special(3).wait;
}

void test_sub2(){
	startSub(2);
	
	fire(1, 16).h(0);
	fire(5, 1).h(0).wide;
	fire(5, 1).snipe.wait;
	
	fire(1, 16).target;
	fire(5, 1).target.snipe.wait;
	
	fire(1, 16).moveLeft;
	fire(1, 16).moveRight;
	fire(1, 16).moveForward;
	fire(5, 1).moveBackward.wait;
	fire(1, 16).jumpLeft;
	fire(5, 1).jumpBackward.wait;
	
	fire(0, 90, 1, 16);
	fire(180, -90, 5, 1).wait;
	fire(A, H, 1, 16);
	fire(H, A, 5, 1).wait;
	
	option(1);
	option(5);
	
	lockon;
	lockonFriendly.type(OKE_BIPED);
	lockonAll;
	
	autoTurn.h(0);
	autoTurn.span(0);
	autoTurn.off;
	
	lockonPart(BODY);
	lockonPart(1);
	lockonPart(5);
	
	lockonId(A);
	lockonId(H);
	
	setAltitude(20);
	setAltitude(100);
}

void chip_main(){
	nop;
	sleep(1);
	sleep(120);
	wait;
	
	A = H;
	H = A;
	A += B;
	B -= C;
	C *= D;
	D /= E;
	E %= F;
	A = 0;
	B = 99999.9;
	C = -99999.9;
	
	A = mathInt(99999.9);
	B = mathAbs(-1);
	C = mathMax(1);
	A = mathMin(H);
	H = mathSqr(A);
	
	chSend(A, 8);
	chSend(H, 1);
	A = chReceive(8);
	H = chReceive(1);
	
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

	If (numAmmo(1) >= 1 && numAmmo(5) > 990) nop; End
	If (numOption(1) <= 1 || numOption(4) < 1) nop; End

	If(isBarrierOver(1).h(0)) nop; End
	If(isBarrierOver(1).w(0)) nop; End
	If(isBarrierOver(1).x(800)) nop; End
	If(isBarrierOver(1).y(800)) nop; End
	If(isBarrierOver(30)) nop; End
	
	If(isBarrierUnder(1).span(0)) nop; End
	If(isBarrierUnder(1).dist(0)) nop; End
	If(isBarrierUnder(30).dir(-180)) nop; End
	If(isBarrierUnder(30).dir(180)) nop; End

	If(isOutsideArea.h(0)) nop; End
	If(isOutsideArea.dir(180)) nop; End
	
	If(numEnemy.h(0) >= 1) nop; End
	If(numEnemy.h(100).type(OKE_BIPED) >= 1) nop; End
	If(numEnemy.h(200).type(OKE_QUADRUPED) >= 1) nop; End
	If(numEnemy.h(400).type(OKE_HOVER) >= 1) nop; End
	If(numEnemy.h(800).type(OKE_VEHICLE) >= 3) nop; End
	If(numEnemy.dist(0).type(OKE_FLIGHT) <= 3) nop; End
	
	If(numProjectile.h(0).type(P_BULLET) >= 1) nop; End
	If(numProjectile.dist(0).type(P_BEAM) > 1) nop; End
	If(numProjectile.type(P_PULSE) <= 1) nop; End
	If(numProjectile.type(P_NAPALM) < 1) nop; End
	If(numProjectile.type(P_GRENADE) >= 1) nop; End
	If(numProjectile.type(P_BOMB) >= 1) nop; End
	If(numProjectile.type(P_ROCKET) >= 1) nop; End
	If(numProjectile.type(P_MISSILE) >= 1) nop; End
	If(numProjectile.type(P_MINE) >= 1) nop; End
	If(numProjectile.type(P_FMINE) >= 1) nop; End
	If(numProjectile.type(P_HI_V) >= 3) nop; End
	If(numProjectile.type(P_ALL) <= 3) nop; End

	If(health >= 0) nop; End
	If(energy <= 0) nop; End
	If(heat <= 100) nop; End
	
	If(isSelfWaiting ) nop; End
	If(isSelfMoving  ) nop; End
	If(isSelfTurning ) nop; End
	If(isSelfJumping ) nop; End
	If(isSelfFiring  ) nop; End
	If(isSelfFighting) nop; End
	If(isSelfSpecial ) nop; End
	If(isSelfStumbling ) nop; End
	If(isUnlock  ) nop; End
	
	If(isTargetStumbling ) nop; End
	
	If(isRand(1)) nop; End
	If(isRand(99)) nop; End
	
	If(isTargetPosition.h(0)) nop; End
	If(isPositionFromTarget.dist(0)) nop; End

	If(A >= B) nop; End
	If(C <= D) nop; End
	If(E == F) nop; End
	If(G == -99999.9) nop; End
	If(H != 99999) nop; End
	
	If(time >= 1) nop; End
	If(time <= 300) nop; End
	If(timeRemained <= 300) nop; End
	
	If(targetBodyCode == BC_UNLOCK) nop; End
	If(targetBodyCode != OKE_FLIGHT) nop; End
	
	If(numLocked != 1) nop; End
	If(numLocked == 3) nop; End
	
	If(targetWeaponId(1) == W_NONE) nop; End
	If(targetWeaponId(2) == W_ASSAULT) nop; End
	If(targetWeaponId(3) == W_BEAM) nop; End
	If(targetWeaponId(4) == W_PULSE) nop; End
	If(targetWeaponId(5) == W_NAPALM) nop; End
	If(targetWeaponId(1) == W_FLAK) nop; End
	If(targetWeaponId(1) == W_SHOTGUN) nop; End
	If(targetWeaponId(1) == W_CANNON) nop; End
	If(targetWeaponId(1) == W_RAILGUN) nop; End
	If(targetWeaponId(1) == W_GRENADE) nop; End
	If(targetWeaponId(1) == W_BOMB) nop; End
	If(targetWeaponId(1) == W_ROCKET) nop; End
	If(targetWeaponId(1) == W_MISSILE) nop; End
	If(targetWeaponId(1) == W_MINE) nop; End
	If(targetWeaponId(1) == W_FMINE) nop; End
	
	If(isLineBlocked) nop; End
	
	test_sub1();
	test_sub2();
}
