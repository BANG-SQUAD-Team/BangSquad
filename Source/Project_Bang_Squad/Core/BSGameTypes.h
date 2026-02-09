// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BSGameTypes.generated.h"

// 전방 선언
class UInputMappingContext;
class UInputAction;

// =============================================================
// [1] 기존 데이터 (복구 완료)
// =============================================================

UENUM(BlueprintType)
enum class EJobType : uint8
{
	None,
	Titan,
	Striker,
	Mage,
	Paladin
};

UENUM(BlueprintType)
enum class EStageIndex : uint8
{
	None,
	Lobby,
	Stage1,
	Stage2,
	Stage3
};

UENUM(BlueprintType)
enum class EStageSection : uint8
{
	Unknown,
	Menu,
	Main,
	MiniGame,
	Boss
};

// =============================================================
// [2] 신규 데이터 (QTE 시스템 추가)
// =============================================================

/** QTE 종류 */
UENUM(BlueprintType)
enum class EQTEType : uint8
{
	None,
	Stage1_Wipe,     // 1스테이지 전멸기 방어 (G키)
	Stage2_Mash,     // 2스테이지 거미줄 탈출 (A/D 연타)
	Stage3_Sequence, // 3스테이지 (예정)
};

/** QTE 설정값 (위젯 + 입력) */
USTRUCT(BlueprintType)
struct FQTEConfig
{
	GENERATED_BODY()

	// 사용할 위젯 (예: WBP_QTE_Mash)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UUserWidget> WidgetClass;

	// 사용할 입력 컨텍스트 (예: IMC_QTE_AD)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UInputMappingContext> InputContext;

	// 감지할 입력 액션 (예: IA_Tap)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UInputAction> InputAction;
};