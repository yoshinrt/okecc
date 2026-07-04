#include "okecc.h"

void use_option(){
	//start_sub(1);
	
	// 冷却
	If(heat() >= 65)
		If(option_num(1))
			option(1);
		Else
			option(2);
		Endif
		
		If(heat() >= 70) option(3); Endif
	Endif
}

void chip_main(){
	for(int i = 0; i < 4; ++i){
		// 格闘
		If(target_z() <= 6 && is_target_direction(0, 160) && target_distance() <= 30)
			strike();
			Return;
		Endif
		
		lockon(0, 512, 320, OKE_ALL);
		
		use_option();
		
		If(
			is_target_direction(0, 64) ||
			is_barrier_over(0, 128, 20, 3) ||
			projectile_num(0, 32, 160, P_ALL)
		)
			turn_left();
			Return;
		Endif
		
		If(
			friendly_num(0, 128, 20, OKE_ALL) ||
			is_barrier_over(0, 160, 40, 24)
		)
			turn_left();
			Return;
		Endif
		
		// ターゲットが後方遠くにいる場合は方向転換
		If(is_target_direction(256, 256) && target_distance() >= 160)
			If(is_target_direction(-128, 256))
				turn_left();
			Else
				turn_right();
			Endif
			Return;
		Endif
		
		// 前進
		move_forward();
		
		B = time();
		C = ch_receive(1);
		
		// ミサイルタイマを過ぎるか，破損が多ければミサイルを撃つ
		If(B >= C || damage() >= 60)
			// ミサイル射撃
			ch_send(B += 4, 1);
			
			If(ammo_num(2))
				fire(0, 512, 320, OKE_ALL, 2, 1);
				wait_ae();
			Endif
			If(ammo_num(3))
				fire(0, 512, 320, OKE_ALL, 3, 1);
				wait_ae();
			Endif
			Return;
		Endif
		
		// 飛行型には積極的にはカノンを撃たない
		If(!(target_z() >= 6 && heat() >= 50))
			While(1)
				B = ammo_num(1);
				If(A != B || !is_self_firing()) Break; Endif
			Endwhile
			
			fire(0, 448, 160, OKE_ALL, 3, 1);
			fire(0, 448, 160, OKE_ALL, 1, 1);
			A = ammo_num(1);
		Endif
	}
}
