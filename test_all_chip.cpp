#include "okecc.h"

void chip_main(){
	nop();
	wait_ae();
	wait(60);
	sound(4, 7);
	move_forward();
	move_backward();
	move_left();
	move_right();
	stop();
	turn_left();
	turn_right();
	jump_forward();
	jump_backward();
	jump_left();
	jump_right();
	strike();
	crouch();
	guard();
	action(1);
	action(2);
	action(3);
	move_high();
	move_mid();
	move_low();
	fire(-240, 32, 20, OKE_BIPED, 1, 8);
	fire(0, 512, 320, OKE_QUADRUPED, 5, 1);
	fire(256, 32, 20, OKE_VEHICLE, 1, 8);
	fire(-240, 32, 20, OKE_HOVER, 1, 8);
	fire(-240, 32, 20, OKE_FLIGHT, 1, 8);
	fire(-240, 32, 20, OKE_ALL, 1, 8);
	fire_direction(-240, -112, 1, 1);
	fire_direction(0, 0, 5, 8);
	fire_direction(256, 112, 5, 8);
	fire_target(1, 1);
	fire_target(5, 8);
	fire_direction(A, F, 1, 1);
	fire_direction(F, A, 5, 8);
	option(1);
	option(2);
	option(3);
	If(ammo_num(1) >= 0) nop(); Endif
	If(ammo_num(1) <= 0) nop(); Endif
	If(ammo_num(5) <= 127) nop(); Endif
	If(option_num(1) <= 127) nop(); Endif
	If(option_num(3) >= 127) nop(); Endif
	If(enemy_num(-240, 32, 20, OKE_BIPED) >= 1) nop(); Endif
	If(enemy_num(0, 512, 320, OKE_QUADRUPED) >= 4) nop(); Endif
	If(enemy_num(256, 32, 20, OKE_VEHICLE) <= 1) nop(); Endif
	If(enemy_num(-240, 32, 20, OKE_HOVER) <= 4) nop(); Endif
	If(enemy_num(-240, 32, 20, OKE_FLIGHT) >= 1) nop(); Endif
	If(friendly_num(-240, 32, 20, OKE_ALL) >= 1) nop(); Endif
	If(is_barrier_over(-240, 32, 10, 3)) nop(); Endif
	If(is_barrier_under(256, 512, 160, 24)) nop(); Endif
	If(projectile_num(-240, 32, 10, P_ALL) >= 0) nop(); Endif
	If(projectile_num(256, 512, 160, P_BULLET) <= 0) nop(); Endif
	If(projectile_num(-240, 32, 10, P_MISSILE) >= 7) nop(); Endif
	If(projectile_num(-240, 32, 10, P_BEAM) <= 7) nop(); Endif
	If(projectile_num(-240, 32, 10, P_ROCKET) >= 0) nop(); Endif
	If(projectile_num(-240, 32, 10, P_MINE) >= 0) nop(); Endif
	If(projectile_num(-240, 32, 10, P_FMINE) >= 0) nop(); Endif
	If(projectile_num(-240, 32, 10, P_HI_V) >= 0) nop(); Endif
	If(is_mappoint(-240, 32, 20, 1, 8)) nop(); Endif
	If(is_mappoint(256, 512, 320, 8, 1)) nop(); Endif
	If(heat() >= 0) nop(); Endif
	If(fuel() <= 100) nop(); Endif
	If(damage() <= 100) nop(); Endif
	If(is_rand(1,2)) nop(); Endif
	If(is_rand(49,50)) nop(); Endif
	If(time() >= 0) nop(); Endif
	If(time() <= 290) nop(); Endif
	lockon(-240, 32, 20, OKE_BIPED);
	lockon(0, 512, 320, OKE_QUADRUPED);
	lockon(256, 32, 20, OKE_VEHICLE);
	lockon(-240, 32, 20, OKE_HOVER);
	lockon(-240, 32, 20, OKE_FLIGHT);
	lockon(-240, 32, 20, OKE_ALL);
	lockon_friendly(-240, 32, 20, OKE_ALL);
	If(target_distance() >= 0) nop(); Endif
	If(target_distance() <= 315) nop(); Endif
	If(is_target_direction(-240, 32)) nop(); Endif
	If(is_target_direction(256, 512)) nop(); Endif
	If(target_x() >= -168) nop(); Endif
	If(target_x() <= -168) nop(); Endif
	If(target_y() >= 0) nop(); Endif
	If(target_y() <= 168) nop(); Endif
	If(self_z() >= 168) nop(); Endif
	If(self_z() <= -168) nop(); Endif
	If(is_target_stop   ()) nop(); Endif
	If(is_self_moving ()) nop(); Endif
	If(is_self_turning()) nop(); Endif
	If(is_self_jumping()) nop(); Endif
	If(is_self_firing ()) nop(); Endif
	If(is_self_acting ()) nop(); Endif
	If(is_target_stun   ()) nop(); Endif
	sound(1, 5);
	sound(5, 1);
	A = fuel();
	B = heat();
	C = damage();
	D = ammo_num(1);
	F = ammo_num(5);
	A = time();
	B = rand();
	C = friendly_num();
	F = enemy_num();
	clamp(A, -127, 127);
	clamp(F, 127, -127);
	clamp(F, 0, 0);
	ch_send(A, 1);
	ch_send(F, 5);
	F = ch_receive(1);
	A = ch_receive(6);
	A += B;
	B -= C;
	C *= D;
	D /= E;
	E %= F;
	F = A;
	A += 0;
	B = 255;
	If(A >= F) nop(); Endif
	If(F <= A) nop(); Endif
	If(B == C) nop(); Endif
	If(D != E) nop(); Endif
	If(A >= 127) nop(); Endif
	If(A <=  -127) nop(); Endif
}
