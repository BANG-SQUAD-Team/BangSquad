#include "Project_Bang_Squad/Character/MageCharacter.h"
#include "Project_Bang_Squad/Projectile/MageProjectile.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/DataTable.h"

AMageCharacter::AMageCharacter()
{
    GetCharacterMovement()->MaxWalkSpeed = 500.f;
    JumpCooldownTimer = 1.0f;   // 1초 쿨타임
    UnlockedStageLevel = 1;     // 현재 진행 중인 스테이지 (테스트용)
}

void AMageCharacter::Attack()
{
	// 데이터 테이블에 "Attack" 이라는 이름의 행이 있어야함
	ProcessSkill(TEXT("Attack"));
}

void AMageCharacter::Skill1()
{
    ProcessSkill(TEXT("Skill1"));
}

void AMageCharacter::Skill2()
{
    ProcessSkill(TEXT("Skill2"));
}

void AMageCharacter::JobAbility()
{
    // 우클릭 직업 능력도 데이터 테이블에서 관리
    ProcessSkill(TEXT("JobAbility"));
}

void AMageCharacter::ProcessSkill(FName SkillRowName)
{
    if (!SkillDataTable)
    {
       UE_LOG(LogTemp, Warning, TEXT("마법사 스킬 데이터 테이블이 할당되지 않았습니다."));
       return;
    }

    // 데이터 테이블에서 행 찾기
    static const FString ContextString(TEXT("SkillContext"));
    FSkillData* Data = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);

    if (Data)
    {
       // 1. 부모 클래스의 함수를 호출하여 스테이지 해금 여부 체크
       if (!IsSkillUnlocked(Data->RequiredStage))
       {
          if (GEngine)
          {
             GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red,
                FString::Printf(TEXT("아직 사용할 수 없습니다! (필요 스테이지: %d)"), Data->RequiredStage));
          }
          return;
       }

       // 2. 몽타주가 있다면 재생
       if (Data->SkillMontage)
       {
          PlayAnimMontage(Data->SkillMontage);
       }

    	// 3. 투사체 소환 로직
    	if (Data->ProjectileClass)
    	{
    		// 위치: 캐릭터의 현재 위치에서 앞방향으로 100유닛(1m) 떨어진 곳
    		FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * 100.0f;
            
    		// 회전: 컨트롤러(카메라)가 바라보는 방향으로 발사
    		FRotator SpawnRotation = GetControlRotation();

    		FActorSpawnParameters SpawnParams;
    		SpawnParams.Owner = this;            // 투사체의 주인은 나
    		SpawnParams.Instigator = GetInstigator();

    		// 월드에 투사체 생성
    		AMageProjectile* Projectile = GetWorld()->SpawnActor<AMageProjectile>(
				Data->ProjectileClass, 
				SpawnLocation, 
				SpawnRotation, 
				SpawnParams
			);

    		if (Projectile)
    		{
    			// [중요] 데이터 테이블에 정의된 데미지를 투사체에 직접 전달
    			Projectile->Damage = Data->Damage;
                
    			UE_LOG(LogTemp, Log, TEXT("%s 생성 완료! 데미지: %.1f"), *Data->ProjectileClass->GetName(), Data->Damage);
    		}
    	}
    	
    	
       // 4. 기능 실행 (로그 출력)
       if (GEngine)
       {
          GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, 
             FString::Printf(TEXT("%s 시전! (데미지: %.1f)"), *Data->SkillName.ToString(), Data->Damage));
       }

       // TODO: 실제 투사체 생성 로직 등이 여기에 들어갈 예정입니다.
    }
    else
    {
       UE_LOG(LogTemp, Warning, TEXT("데이터 테이블에서 행 [%s]를 찾을 수 없습니다."), *SkillRowName.ToString());
    }
}