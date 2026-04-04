#include "okecc.h"

void use_option(){
	//start_sub(1);
	
	// 冷却
	if(heat() >= 65)
		if(option_num(1))
			option(1);
		else
			option(2);
		endif
		
		if(heat() >= 70) option(3); endif
	endif
}

void chip_main(){
	for(int i = 0; i < 4; ++i){
		// 格闘
		if(target_z() <= 6 && is_target_direction(0, 160) && target_distance() <= 30)
			fight();
			exit();
		endif
		
		lockon(0, 512, 320, OKE_ALL);
		
		use_option();
		
		if(
			is_target_direction(0, 64) ||
			is_barrier_over(0, 128, 20, 3) ||
			projectile_num(0, 32, 160, P_ALL)
		)
			turn_left();
			exit();
		endif
		
		if(
			friendly_num(0, 128, 20, OKE_ALL) ||
			is_barrier_over(0, 160, 40, 24)
		)
			turn_left();
			exit();
		endif
		
		// ターゲットが後方遠くにいる場合は方向転換
		if(is_target_direction(256, 256) && target_distance() >= 160)
			if(is_target_direction(-128, 256))
				turn_left();
			else
				turn_right();
			endif
			exit();
		endif
		
		// 前進
		move_forward();
		
		B = time();
		C = ch_receive(1);
		
		// ミサイルタイマを過ぎるか，破損が多ければミサイルを撃つ
		if(B >= C || damage() >= 60)
			// ミサイル射撃
			ch_send(B += 4, 1);
			
			if(ammo_num(2))
				fire(0, 512, 320, OKE_ALL, 2, 1);
				wait_ae();
			endif
			if(ammo_num(3))
				fire(0, 512, 320, OKE_ALL, 3, 1);
				wait_ae();
			endif
			exit();
		endif
		
		// 飛行型には積極的にはカノンを撃たない
		if(!(target_z() >= 6 && heat() >= 50))
			loop
				B = ammo_num(1);
				if(A != B || !is_self_firing()) break; endif
			endloop
			
			fire(0, 448, 160, OKE_ALL, 3, 1);
			fire(0, 448, 160, OKE_ALL, 1, 1);
			A = ammo_num(1);
		endif
	}
}
