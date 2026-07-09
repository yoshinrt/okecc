#include "okecc.h"

void chip_main(){
#if 0
	nop;
	//wait(1);
	//wait(120);
	//wait();
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
	
	A = math_int(99999.9);
	B = math_abs(-1);
	C = math_max(1);
	A = math_min(H);
	H = math_sqr(A);
	
	ch_send(A, 8);
	ch_send(H, 1);
	A = ch_receive(8);
	H = ch_receive(1);
	
	//A = num_enemy();
	//H = num_friendly();
	A = time();
	A = math_rand();
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

	If (num_ammo(1) >= 1) nop; Endif
	If (num_ammo(5) >= 990) nop; Endif
	If (num_option(1) <= 1) nop; Endif
	If (num_option(4) <= 1) nop; Endif

	If(is_barrier_over(1).h(0)) nop; Endif
	If(is_barrier_over(1).w(0)) nop; Endif
	If(is_barrier_over(1).x(800)) nop; Endif
	If(is_barrier_over(1).y(800)) nop; Endif
	If(is_barrier_over(30)) nop; Endif
	
	If(is_barrier_under(1).span(0)) nop; Endif
	If(is_barrier_under(1).dist(0)) nop; Endif
	If(is_barrier_under(30).dir(-180)) nop; Endif
	If(is_barrier_under(30).dir(180)) nop; Endif

	If(is_outside_area.h(0)) nop; Endif
	If(is_outside_area.dir(180)) nop; Endif
	
	If(num_enemy.h(0) >= 1) nop; Endif
	If(num_enemy.h(100).type(OKE_BIPED) >= 1) nop; Endif
	If(num_enemy.h(200).type(OKE_QUADRUPED) >= 1) nop; Endif
	If(num_enemy.h(400).type(OKE_HOVER) >= 1) nop; Endif
	If(num_enemy.h(800).type(OKE_VEHICLE) >= 3) nop; Endif
	If(num_enemy.dist(0).type(OKE_FLIGHT) <= 3) nop; Endif
	
	If(num_projectile.h(0).type(P_BULLET) >= 1) nop; Endif
	If(num_projectile.dist(0).type(P_BEAM) > 1) nop; Endif
	If(num_projectile.type(P_PULSE) <= 1) nop; Endif
	If(num_projectile.type(P_NAPALM) < 1) nop; Endif
	If(num_projectile.type(P_GRENADE) >= 1) nop; Endif
	If(num_projectile.type(P_BOMB) >= 1) nop; Endif
	If(num_projectile.type(P_ROCKET) >= 1) nop; Endif
	If(num_projectile.type(P_MISSILE) >= 1) nop; Endif
	If(num_projectile.type(P_MINE) >= 1) nop; Endif
	If(num_projectile.type(P_FMINE) >= 1) nop; Endif
	If(num_projectile.type(P_HI_V) >= 3) nop; Endif
	If(num_projectile.type(P_ALL) <= 3) nop; Endif

	If(health() >= 0) nop; Endif
	If(energy() <= 0) nop; Endif
	If(heat() <= 100) nop; Endif
	
	If(is_self_waiting ()) nop; Endif
	If(is_self_moving  ()) nop; Endif
	If(is_self_turning ()) nop; Endif
	If(is_self_jumping ()) nop; Endif
	If(is_self_firing  ()) nop; Endif
	If(is_self_fighting()) nop; Endif
	If(is_self_special ()) nop; Endif
	If(is_self_stunble ()) nop; Endif
	If(is_self_unlock  ()) nop; Endif
	
	If(is_target_stunble ()) nop; Endif
	
	If(is_rand(1)) nop; Endif
	If(is_rand(99)) nop; Endif
	
	If(is_target_position.h(0)) nop; Endif
	If(is_position_from_target.dist(0)) nop; Endif

	If(A >= B) nop; Endif
	If(C <= D) nop; Endif
	If(E == F) nop; Endif
	If(G == -99999.9) nop; Endif
	If(H == 99999) nop; Endif
	If(time() >= 1) nop; Endif
	If(time() <= 300) nop; Endif
	If(time_remained() <= 300) nop; Endif
	
	If(bodycode() == 0) nop; Endif
	If(bodycode() != 37) nop; Endif
	
#endif
	If(num_locked() != 1) nop; Endif
	If(num_locked() == 3) nop; Endif
#if 0
	stop;
	
	move_forward;
	move_backward;
	move_left;
	move_right;
	
	turn_left;
	turn_right;
	
	jump_forward;
	jump_backward;
	jump_left.wait;
	jump_right.wait;
	
	move_forward.fast;
	move_backward.fast;
	move_left.fast.wait;
	move_right.fast.wait;
	
	turn_left.fast;
	turn_right.fast.wait;
	
	fight_low;
	fight_high;
	fight_long;
	fight.wait;
	
	guard(5);
	crouch(60).wait;
	
	special(1);
	special(3).wait;
	
	fire(1, 16).h(0);
	fire(5, 1).h(0).wide;
	fire(5, 1).snipe.wait;
	
	fire(1, 16).target;
	
	option(1);
	option(5);
	
	lockon;
	lockon_friendly.type(OKE_BIPED);
	lockon_all;
	
	set_altitude(20);
	set_altitude(100);
#endif
}
