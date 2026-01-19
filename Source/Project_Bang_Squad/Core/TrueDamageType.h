#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h" // 부모 클래스 헤더
#include "TrueDamageType.generated.h"

/**
 * 방어 무시, 무적 무시, 즉사기 전용 데미지 타입
 * 로직 검사 예시: if (DamageEvent.DamageTypeClass == UTrueDamageType::StaticClass())
 */
UCLASS()
class PROJECT_BANG_SQUAD_API UTrueDamageType : public UDamageType
{
    GENERATED_BODY()

    // 특별한 변수나 함수가 필요 없습니다.
    // 이 클래스의 존재 자체가 "트루 데미지"라는 증거가 됩니다.
};