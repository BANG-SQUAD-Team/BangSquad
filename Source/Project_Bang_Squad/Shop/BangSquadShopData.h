// BangSquadShopData.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h" // 데이터 테이블 기능을 위해 필수
#include "BangSquadShopData.generated.h"

/** * 아이템 등급 (Rarity)
 * - UI에서 배경색이나 테두리 색상을 결정할 때 사용합니다.
 */
UENUM(BlueprintType)
enum class EItemRarity : uint8
{
	Common		UMETA(DisplayName = "Common (일반)"),
	Rare		UMETA(DisplayName = "Rare (희귀)"),
	Epic		UMETA(DisplayName = "Epic (영웅)"),
	Legendary	UMETA(DisplayName = "Legendary (전설)")
};

/**
 * 아이템 타입 (Type)
 * - HeadGear: 캐릭터 외형을 바꾸는 장식 (소켓에 부착)
 * - StatUpgrade: 캐릭터 능력치를 올리는 강화 (공격력, 이속 등)
 */
UENUM(BlueprintType)
enum class EItemType : uint8
{
	HeadGear	UMETA(DisplayName = "Head Accessory (머리 장식)"),
	StatUpgrade	UMETA(DisplayName = "Stat Upgrade (능력 강화)")
};

/**
 * [하위 구조체] 레벨별 강화 정보
 * - 능력 강화 아이템(StatUpgrade)일 때만 사용합니다.
 * - 예: Lv1 데이터, Lv2 데이터...
 */
USTRUCT(BlueprintType)
struct FShopItemLevelInfo
{
	GENERATED_BODY()

public:
	// 이 레벨로 올리는 데 필요한 비용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade")
	int32 UpgradeCost;

	// 이 레벨일 때 적용되는 능력치 (누적치가 아니라, 해당 레벨의 최종 수치 추천)
	// 예: Lv1=10.0, Lv2=25.0
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade")
	float StatValue;
};

/**
 * [메인 구조체] 상점 아이템 데이터
 * - FTableRowBase를 상속받아 데이터 테이블에서 행(Row)으로 사용 가능합니다.
 */
USTRUCT(BlueprintType)
struct FShopItemData : public FTableRowBase
{
	GENERATED_BODY()

public:
	// ---------------------------------------------------
	// 1. 공통 데이터 (모든 아이템이 가짐)
	// ---------------------------------------------------

	// 아이템 이름 (UI 표시용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Info")
	FText ItemName;

	// 아이템 설명
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Info", meta = (MultiLine = true))
	FText Description;

	// 아이콘 (UI 표시용 이미지)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Info")
	UTexture2D* Icon;

	// 아이템 타입 (장식 vs 강화)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Info")
	EItemType ItemType;

	// ---------------------------------------------------
	// 2. 머리 장식(HeadGear) 전용 데이터
	// - ItemType이 HeadGear일 때만 값을 채우세요.
	// ---------------------------------------------------

	// 장식 아이템의 등급
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Info")
	EItemRarity Rarity;

	// 장식 아이템의 가격 (단일 구매이므로 기본 가격 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Info")
	int32 BasePrice;

	// 장착할 스태틱 메쉬 (소켓에 붙을 모델링)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Info")
	UStaticMesh* HeadMesh;

	// ---------------------------------------------------
	// 3. 능력 강화(StatUpgrade) 전용 데이터
	// - ItemType이 StatUpgrade일 때만 값을 채우세요.
	// - 데이터 테이블에서 배열 요소(+)를 추가하여 레벨을 만듭니다.
	// ---------------------------------------------------

	// 레벨별 가격과 능력치 목록
	// Index 0 = 1레벨 정보, Index 1 = 2레벨 정보...
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade Info")
	TArray<FShopItemLevelInfo> LevelData;
};