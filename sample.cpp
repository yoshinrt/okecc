// シナリオモードのラスティネイル用ソフト

#include "okecc.h"

#define prev_ammo_num	A
#define missile_timer	B
#define cur_time		C
#define overheat		D

void useOption(){
	
	// ミサイル妨害
	If(numProjectile.dist(50).type(P_MISSILE))
		option(1);
	End
	
	// 冷却
	If(heat >= 60)
		option(3);
	End
	
	// 修復
	If(health <= 50)
		option(2);
	End
}

void chip_main(){
	setCpuSize(10);
	
	useOption();
	lockon;
	
	// 格闘
	If(numEnemy.span(90).dist(50) && targetBodyCode != OKE_FLIGHT)
		fight;
		Return;
	End
	
	// 冷却 op が 0 のときの挙動:
	//   heat が 60 を超えたら 45 まで攻撃しない
	If(!numOption(3))
		If(overheat)
			If(heat <= 45)
				overheat = 0;
			End
		Elseif(heat >= 60)
			overheat = 1;
		End
	End
	
	// 敵弾に対する反応
	If(numProjectile.span(90).dist(80).type(P_HI_V))
			
		// 超短距離に敵弾: ガード
		If(numProjectile.span(90).dist(20).type(P_HI_V))
			guard(15);
			
		Elseif(!isSelfJumping)
			If(isRand(50))
				jumpLeft;
			Else
				jumpRight;
			End
		End
		
		Return;
	End
	
	// 移動
	If(!isTargetPosition.span(120))
		If(isTargetPosition.dir(-90).span(180))
			turnLeft;
		Else
			turnRight;
		End
		Return;
	End
	
	// ミサイル
	If(missile_timer <= (cur_time = time) && isTargetPosition.dist(220) && numAmmo(2))
		wait;
		(missile_timer = time) += 4;
		fire(2, 1).target.wide.wait;
		Return;
	End
	
	// カノン
	If(isTargetPosition.dist(200).span(140))
		If(!overheat)
			// カノン発射
			fire(1, 2).target.snipe;
		End
	Else
		moveForward;
	End
}
