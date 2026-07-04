/*
+[機体名 ] かのんべいべー１Ｄ
[OKE CODE] CBAB1D                    重量   VP
[BODY    ] バッドドリーム            5000  1000
[CPU     ] MCP112(中速)               100   200
[Armor   ] 70mm                      5000   300
[EX Armor] 対徹甲                    1000   300
[Weapon 1] 185mmカノン        x  70  1260   840
[Weapon 2] ムラマサ           x   4   480   480
[Weapon 3] ラプトル           x   8   440   560
[Option 1] 機体冷却装置               100   100
[Option 2] 機体冷却装置               100   100
[Option 3] 誘導妨害装置               150   150
[Total   ]                    19200/13630  4030
[Loadage ]                       70.99% (白 / 白)
[Paint   ] 塗装パターン4     (10,8,16) (4,4,6)
*/

#include "okecc.h"

#define cur_time			A
#define cannon_timer		B
#define missile_timer		C
#define prev_cannon_ammo	D

void move(){
	start_sub(1);
	
	If(is_barrier_over(0, 384, 20, 3))
		move_backward();
		
	Elseif(
		enemy_num(256, 64, 240, OKE_ALL) ||
		is_barrier_over(256, 96, 40, 24)
	)
		turn_left();
		
	Elseif(is_barrier_over(256, 96, 20, 3))
		If(cur_time >= 3)
			turn_right();
		Else
			turn_left();
		Endif
		
	Elseif(
		enemy_num(-144, 448, 320, OKE_ALL) ||
		projectile_num(112, 64, 160, P_ALL) ||
		enemy_num(0, 512, 80, OKE_ALL)
	)
		move_backward();
		
	Elseif(cur_time < 10)
		cannon_timer = 10;
		move_backward();
	Else
		move_forward();
	Endif
}

void fire_missile(int oke_type){
	If(ammo_num(2))
		fire(0, 512, 320, oke_type, 2, 1);
	Elseif(ammo_num(3))
		fire(0, 512, 320, oke_type, 3, 1);
	Endif
}

void chip_main(){
	
	lockon(0, 512, 320, OKE_ALL);
	
	// 格闘
	If(target_z() <= 6 && target_distance() <= 30 && is_target_direction(0, 160))
		strike();
		Return;
	Endif
	
	cur_time = time();
	
	move();
	
	// 冷却
	If(heat() > 70)
		If(option_num(1) >= 1)
			option(1);
		Else
			option(2);
		Endif
		
		// 冷却中は移動に徹する
		(cannon_timer = cur_time) += 4;
		(missile_timer = cur_time) += 2;
	Endif
	
	// ミサイル妨害
	If(projectile_num(0, 320, 50, P_MISSILE))
		option(3);
	Endif
	
	// カノン
	If(cur_time >= cannon_timer && ammo_num(1))
		
		get_target_direction(E, F);
		If(F <= 32)
			While(is_self_firing() && prev_cannon_ammo == (F = ammo_num(1)))
			Endwhile
			
			// 被弾したら 3秒間移動
			If(is_self_stun())
				(cannon_timer = cur_time) += 3;
				missile_timer = cannon_timer;
				Return;
			Endif
			
			fire(0, 448, 160, OKE_ALL, 3, 1);
			fire(0, 448, 160, OKE_ALL, 1, 1);
			prev_cannon_ammo = ammo_num(1);
		Endif
	Endif
	
	// ミサイル
	If(cur_time >= missile_timer && is_self_moving())
		(missile_timer = time()) += 4;
		
		If(enemy_num(0, 512, 320, OKE_FLIGHT))
			fire_missile(OKE_FLIGHT);
		Else
			fire_missile(OKE_ALL);
		Endif
	Endif
}
