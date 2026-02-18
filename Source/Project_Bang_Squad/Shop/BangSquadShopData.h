#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BangSquadShopData.generated.h"

/** 직업 목록 (드롭다운용) */
UENUM(BlueprintType)
enum class ECharacterJob : uint8
{
	Common		UMETA(DisplayName = "All Jobs"), // 공용 아이템
	Mage		UMETA(DisplayName = "Mage"),
	Striker		UMETA(DisplayName = "Striker"),
	Titan		UMETA(DisplayName = "Titan"),
	Paladin		UMETA(DisplayName = "Paladin")
};

/** 아이템 등급 */
UENUM(BlueprintType)
enum class EItemRarity : uint8
{
	Common		UMETA(DisplayName = "Common"),
	Rare		UMETA(DisplayName = "Rare"),
	Epic		UMETA(DisplayName = "Epic"),
	Legendary	UMETA(DisplayName = "Legendary")
};

/** 아이템 타입 (스탯 제거, 스킨 추가) */
UENUM(BlueprintType)
enum class EItemType : uint8
{
	None		UMETA(DisplayName = "None"),
	HeadGear	UMETA(DisplayName = "Head Accessory"), // 머리 장식
	Skin		UMETA(DisplayName = "Character Skin")  // 캐릭터 스킨 (재질 변경)
};

/** [메인 구조체] 상점 아이템 데이터 */
USTRUCT(BlueprintType)
struct FShopItemData : public FTableRowBase
{
	GENERATED_BODY()

public:
	// --- 기본 정보 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Info")
	FText ItemName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Info", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Info")
	UTexture2D* Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common Info")
	EItemType ItemType;

	// ★ [핵심] 직업 제한 (드롭다운으로 선택 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requirement")
	ECharacterJob RequiredJob = ECharacterJob::Common;

	// --- 장식/가격 정보 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Info")
	EItemRarity Rarity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic Info")
	int32 Price;

	// --- 1. 부착형 장식 (투구 등) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual - HeadGear")
	UStaticMesh* StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual - HeadGear")
	USkeletalMesh* SkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual - HeadGear")
	UAnimationAsset* IdleAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual - HeadGear")
	FTransform AdjustTransform = FTransform::Identity;

	// --- 2. 스킨형 (재질 변경) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual - Skin")
	UMaterialInterface* SkinMaterial;
};