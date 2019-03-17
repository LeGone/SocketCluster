#pragma once

#include "CoreMinimal.h"

#include "Runtime/Launch/Resources/Version.h"

#if ENGINE_MINOR_VERSION >= 15
#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "Engine/Engine.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#else
#include "CoreUObject.h"
#include "Engine.h"
#endif

#include "Sockets.h"
#include "SocketSubsystem.h"

#include "Map.h"
#include "Json.h"

#include "Tickable.h"
#include "LatentActions.h"
#include "SharedPointer.h"

// Namespace UI Conflict.
// Remove UI Namepspace
#if PLATFORM_LINUX
#pragma push_macro("UI")
#undef UI
#elif PLATFORM_WINDOWS || PLATFORM_MAC
#define UI UI_ST
#endif 

THIRD_PARTY_INCLUDES_START
#include <iostream>
#include "PreWindowsApi.h"
#include "libwebsockets.h"
#include "PostWindowsApi.h" 
THIRD_PARTY_INCLUDES_END

// Namespace UI Conflict.
// Restore UI Namepspace
#if PLATFORM_LINUX
#pragma pop_macro("UI")
#elif PLATFORM_WINDOWS || PLATFORM_MAC
#undef UI
#endif 

#include "SocketCluster.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(SocketClusterLog, Log, All);

DECLARE_DELEGATE_TwoParams(FResponseCallback, FString, UObject*);

UCLASS()
class USocketCluster : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:

	// Initialize SocketCluster Class.
	USocketCluster();

	/*
	~USocketCluster()
	{
		lws_context_destroy(LwsContext);
		delete LwsContext;
		delete Lws;
	}
	*/

	void CreateContext();

	void Connect(UWorld* World, FString &Url);
	void Disconnect();

	// Override Tick Event.
	virtual void Tick(float DeltaTime) override;

	// Override IsTickable Event.
	virtual bool IsTickable() const override;

	// Override GetStatId Event.
	virtual TStatId GetStatId() const override;

	// WebSocket Service Callback Function
	static int WsCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

	// Websocket Service WriteBack Function
	static int WsWriteBack(struct lws *wsi, const char *str, int str_size_in);

	/* Emit To SocketCluster Server */
	void Emit(const FString& Event, const FString& Data, const FResponseCallback& Callback, UObject* Sender = nullptr);

	void WriteBuffer(struct lws * wsi);

	static float AckTimeout;

	int CID;

	UWorld* World;

	lws* Lws;
	lws_context* LwsContext;
	lws_protocols* Protocols;

	TArray<TSharedPtr<FJsonObject>> Send;
	TMap<int, TTuple<FResponseCallback, UObject*>> Responses;
};