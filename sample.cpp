// てきとうな手動操作 OKE

// 操作方法
// 移動方向はアナログスティック
#define BT_FIGHT	(BT_CIRCLE		| BT_HELD_ALL)	// ◯ 格闘
#define BT_JUMP		(BT_CROSS		| BT_HELD_ALL)	// ✕ ジャンプ
#define BT_FIRE		(BT_R			| BT_HELD_ALL)	// R  武装1 発射
#define BT_FIRE2	(BT_L			| BT_HELD_ALL)	// L  武装2 発射
#define BT_SEL_TGT	(BT_TRIANGLE	| BT_PUSHED)	// △ ターゲット変更

#include "okecc.h"

void chip_main(){
	
	// 移動
	#define anX	A
	#define anY	B
	
	getButton;
	anX = analogX;
	anY = analogY;
	
	#define AN_TH	90
	
	// 格闘
	If(isButton(BT_FIGHT))
		fight;
		Return;
	End
	
	// ターゲット変更
	If(!isLocked)
		lockon;
		gunsightOn(1);
	ElseIf(isButton(BT_SEL_TGT))
		lockon.front;
	End
	
	// 射撃
	If(isButton(BT_FIRE))
		fire(3);
	End
	
	If(isButton(BT_FIRE2))
		fire(2, 1).gunsight;
	End
	
	// 移動 or 前後ジャンプ
	If(anY < -AN_TH)
		If(isButton(BT_JUMP))
			jumpForward;
		Else
			moveForward;
		End
	ElseIf(anY > AN_TH)
		If(isButton(BT_JUMP))
			jumpBackward;
		Else
			moveBackward;
		End;
	Else
		stopMove;
	End
	
	// 旋回 or 左右ジャンプ
	If(anX < - AN_TH)
		If(isButton(BT_JUMP))
			stopTurn;
			jumpLeft;
		Else
			turnLeft;
		End
	ElseIf(anX > AN_TH)
		If(isButton(BT_JUMP))
			stopTurn;
			jumpRight;
		Else
			turnRight;
		End
	Else
		stopTurn;
	End
	
	nop;
	
	// その場ジャンプ
	If(anX > -AN_TH && anX < AN_TH && anY > -AN_TH && anY < AN_TH && isButton(BT_JUMP))
		jumpUp;
	End
}
