#include "okecc.h"

void chip_main(){
#if 0
	nop();
	wait(1);
	wait(120);
	wait();
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
	
	A = oke_int(99999.9);
	B = oke_abs(-1);
	C = oke_max(1);
	A = oke_min(H);
	H = oke_sqr(A);
	
	ch_send(A, 8);
	ch_send(H, 1);
	A = ch_receive(8);
	H = ch_receive(1);
	
	A = enemy_num();
	H = friendly_num();
	A = time();
	A = rand();
	A = my_x();
	A = my_y();
	A = my_z();
	A = my_direction();
	A = target_no();
	A = target_azimuth();
	A = target_elevation();
	A = target_x();
	A = target_y();
	A = target_z();
	A = target_direction();
	A = target_bodycode();
	A = target_actcode();
	A = target_distance();
	A = target_distance_xy();

	If (ammo_num(1) >= 1) nop(); Endif
	If (ammo_num(5) >= 990) nop(); Endif
	If (option_num(1) <= 1) nop(); Endif
	If (option_num(4) <= 1) nop(); Endif

	If(is_barrier_over({.h = 0})) nop(); Endif
	If(is_barrier_over({.w = 0})) nop(); Endif
	If(is_barrier_over({.x = 800})) nop(); Endif
	If(is_barrier_over({.y = 800})) nop(); Endif
	If(is_barrier_over({.height = 30})) nop(); Endif
	
	If(is_barrier_under({.span = 0})) nop(); Endif
	If(is_barrier_under({.dist = 0})) nop(); Endif
	If(is_barrier_under({.dir = -180})) nop(); Endif
	If(is_barrier_under({.dir = 180})) nop(); Endif

	If(is_outside_area({.h = 0})) nop(); Endif
	If(is_outside_area({.dir = 180})) nop(); Endif
	
	If(enemy_num({.h = 0}) >= 1) nop(); Endif
	If(enemy_num({.h = 100, .type = OKE_BIPED}) >= 1) nop(); Endif
	If(enemy_num({.h = 200, .type = OKE_QUADRUPED}) >= 1) nop(); Endif
	If(enemy_num({.h = 400, .type = OKE_HOVER}) >= 1) nop(); Endif
	If(enemy_num({.h = 800, .type = OKE_VEHICLE}) >= 3) nop(); Endif
	If(enemy_num({.dist = 0, .type = OKE_FLIGHT}) <= 3) nop(); Endif
	
	If(projectile_num({.h = 0, .type = P_BULLET}) >= 1) nop(); Endif
	If(projectile_num({.dist = 0, .type = P_BEAM}) >= 1) nop(); Endif
	If(projectile_num({.type = P_PULSE}) >= 1) nop(); Endif
	If(projectile_num({.type = P_NAPALM}) >= 1) nop(); Endif
	If(projectile_num({.type = P_GRENADE}) >= 1) nop(); Endif
	If(projectile_num({.type = P_BOMB}) >= 1) nop(); Endif
	If(projectile_num({.type = P_ROCKET}) >= 1) nop(); Endif
	If(projectile_num({.type = P_MISSILE}) >= 1) nop(); Endif
	If(projectile_num({.type = P_MINE}) >= 1) nop(); Endif
	If(projectile_num({.type = P_FMINE}) >= 1) nop(); Endif
	If(projectile_num({.type = P_HI_V}) >= 3) nop(); Endif
	If(projectile_num({.type = P_ALL}) <= 3) nop(); Endif

	If(health() >= 0) nop(); Endif
	If(energy() <= 0) nop(); Endif
	If(heat() <= 100) nop(); Endif
	
	If(is_self_waiting ()) nop(); Endif
	If(is_self_moving  ()) nop(); Endif
	If(is_self_turning ()) nop(); Endif
	If(is_self_jumping ()) nop(); Endif
	If(is_self_firing  ()) nop(); Endif
	If(is_self_fighting()) nop(); Endif
	If(is_self_special ()) nop(); Endif
	If(is_self_stunble ()) nop(); Endif
	If(is_self_unlock  ()) nop(); Endif
	
	If(is_target_stunble ()) nop(); Endif
	
	If(is_rand(1)) nop(); Endif
	If(is_rand(99)) nop(); Endif
	
	If(target_position({.h = 0})) nop(); Endif
	If(position_from_target({.dist = 0})) nop(); Endif

	If(A >= B) nop(); Endif
	If(C <= D) nop(); Endif
	If(E == F) nop(); Endif
	If(G == -99999.9) nop(); Endif
	If(H == 99999) nop(); Endif
	
	stop();
	
	move_forward();
	move_backward();
	move_left();
	move_right();
	
	turn_left();
	turn_right();
	
	jump_forward();
	jump_backward();
	jump_left();
	jump_right(WAIT);
	
	move_forward(FAST);
	move_backward(FAST);
	move_left(FAST, WAIT);
	move_right(FAST, WAIT);
	
	turn_left(FAST);
	turn_right(FAST, WAIT);
	
	fight_low();
	fight_high();
	fight_long();
	fight(WAIT);
	
	guard(5);
	crouch(60, WAIT);
	
	special(1);
	special(3, WAIT);
	
	fire({.h = 0}, 1, 16);
	fire({.h = 0, .fire = WIDE}, 5, 1, WAIT);
	fire({.h = 0, .fire = SNIPE}, 5, 1, WAIT);
	
	option(1);
	option(5);
	
	lockon({});
	lockon_friendly({});
	lockon_oke({});
	
	set_altitude(20);
	set_altitude(100);
#endif
	
	fire_target({.fire = WIDE}, 1, 16);
	fire_target({.fire = SNIPE}, 5, 1, WAIT);
}
