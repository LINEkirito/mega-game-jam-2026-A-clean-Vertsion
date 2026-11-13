#include "DataTransmitter.h"
#include "Combat/ImpactBloodEffect.h"
#include "GlobalData.h"
#include "Combat/StatueTargetComponent.h"
#include "Combat/PatrolEnemy.h"
#include "Game_Jam_Clean_VersCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
bool UDataTransmitter::IsHeadHit(const FHitResult& Hit){
 if(!Hit.GetActor())return false;
 if(Hit.BoneName.ToString().Contains(TEXT("head"),ESearchCase::IgnoreCase))return true;
 // Capsule traces do not supply a bone: use the animated head position, never the capsule's upper percentage.
 if(auto* C=Cast<ACharacter>(Hit.GetActor()))if(auto* M=C->GetMesh())if(M->DoesSocketExist(TEXT("head"))){const FVector Head=M->GetSocketLocation(TEXT("head"));return FVector::DistSquared(Hit.ImpactPoint,Head)<=FMath::Square(18.f);}
 return false;
}
float UDataTransmitter::TransmitHit(const FHitResult& Hit,float Base,const FVector& Dir,AController* Instigator,AActor* Source){
 auto* Target=Hit.GetActor();if(!IsValid(Target)||Base<=0)return 0;bool Head=IsHeadHit(Hit);float D=Base*(Head?GetDefault<UGlobalData>()->HeadMultiplier:1.f);
 const auto* NPC=Cast<APatrolEnemy>(Target);const bool Protected=NPC&&NPC->IsProtectedPerson();
 auto* Statue=Target->FindComponentByClass<UStatueTargetComponent>();float Applied=Statue?Statue->ApplyHit(D):UGameplayStatics::ApplyPointDamage(Target,D,Dir,Hit,Instigator,Source,nullptr);
 bool Killed=false;if(Statue)Killed=Statue->Health<=0;if(auto* E=Cast<APatrolEnemy>(Target))Killed=E->Health<=0;if(auto* P=Cast<AGame_Jam_Clean_VersCharacter>(Target))Killed=P->Health<=0;
 if(Applied>0 && Cast<APatrolEnemy>(Target))AImpactBloodEffect::SpawnForHit(GetWorld(),Hit,Dir);
 if(Applied>0){OnCombatDamage.Broadcast(Target,Source,Applied,Head,Killed);OnViolence.Broadcast(Target,Instigator?Instigator->GetPawn().Get():Cast<APawn>(Source),Protected);}return Applied;
}

