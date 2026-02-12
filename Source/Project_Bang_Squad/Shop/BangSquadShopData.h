// BangSquadShopData.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BangSquadShopData.generated.h"

/** 아이템 등급 */
UENUM(BlueprintType)
enum class EItemRarity : uint8
{
	Common		UMETA(DisplayName = "Common"),
	Rare		UMETA(DisplayName = "Rare"),
	Epic		UMETA(DisplayName = "Epic"),
	Legendary	UMETA(DisplayName = "Legendary")
};

/** 아이템 타입 */
UENUM(BlueprintType)
enum class EItemType : uint8
{
	HeadGear	UMETA(DisplayName = "Head Accessory"),
	StatUpgrade	UMETA(DisplayName = "Stat Upgrade")
};

/** [하위 구조체] 레벨별 강화 정보 */
USTRUCT(BlueprintType)
struct FShopItemLevelInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade")
	int32 UpgradeCost;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade")
	float StatValue;
};

/** [메인 구조체] 상점 아이템 데이터 */
USTRUCT(BlueprintType)
struct FShopItemData : public FTableRowBase
{
	GENERATED_BODY()

public:
	// --- 공통 데이터 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Info")
	FText ItemName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Info", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Info")
	UTexture2D* Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Info")
	EItemType ItemType;

	// --- 장식 전용 데이터 (수정됨!) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Info")
	EItemRarity Rarity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Info")
	int32 BasePrice;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Visual")
	UStaticMesh* StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Visual")
	USkeletalMesh* SkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Visual")
	UAnimationAsset* IdleAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Visual")
	FTransform AdjustTransform = FTransform::Identity;

	// --- 능력 강화 데이터 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade Info")
	TArray<FShopItemLevelInfo> LevelData;
};