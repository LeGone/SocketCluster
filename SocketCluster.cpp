// #include "ShooterGame.h"
#include "SocketCluster.h"

#include <iostream>
#include "PreWindowsApi.h"
#include "libwebsockets.h"
#include "PostWindowsApi.h" 

DEFINE_LOG_CATEGORY(SocketClusterLog);

float USocketCluster::AckTimeout;

USocketCluster::USocketCluster()
{
	Protocols = new lws_protocols[3];
	FMemory::Memzero(Protocols, sizeof(lws_protocols) * 3);

	Protocols[0].name = "websocket";
	Protocols[0].callback = USocketCluster::WsCallback;
	Protocols[0].per_session_data_size = 64 * 1024;
	Protocols[0].rx_buffer_size = 0;

	LwsContext = nullptr;
	CID = 1;
}

static void printWebSocketLog(int level, const char *line)
{
	static const char * const log_level_names[] =
	{
		"ERR",
		"WARN",
		"NOTICE",
		"INFO",
		"DEBUG",
		"PARSER",
		"HEADER",
		"EXTENSION",
		"CLIENT",
		"LATENCY",
	};

	char buf[30] = { 0 };
	int n;

	for (n = 0; n < LLL_COUNT; n++)
	{
		if (level != (1 << n))
			continue;
		snprintf(buf, sizeof(buf), "%s: ", log_level_names[n]);
		break;
	}

	FString flin = UTF8_TO_TCHAR(line);
	FString fbuf = UTF8_TO_TCHAR(buf);

	UE_LOG(SocketClusterLog, Log, TEXT("%s%s"), *fbuf, *flin);
}

void USocketCluster::CreateContext()
{
	struct lws_context_creation_info info;
	memset(&info, 0, sizeof info);

	info.protocols = &Protocols[0];
	info.ssl_cert_filepath = NULL;
	info.ssl_private_key_filepath = NULL;

	info.port = -1;
	info.gid = -1;
	info.uid = -1;

	info.options = LWS_SERVER_OPTION_VALIDATE_UTF8 | LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT | LWS_SERVER_OPTION_PEER_CERT_NOT_REQUIRED;

	// int log_level = LLL_ERR | LLL_WARN | /* LLL_NOTICE | LLL_INFO | LLL_DEBUG | */ LLL_PARSER | LLL_HEADER | LLL_EXT | LLL_CLIENT | LLL_LATENCY | LLL_USER;
	// lws_set_log_level(log_level, printWebSocketLog);

	LwsContext = lws_create_context(&info);

	if (LwsContext == nullptr)
	{
		UE_LOG(SocketClusterLog, Error, TEXT("Unable to create lws_context"));
	}
}

void USocketCluster::Connect(UWorld* NewWorld, FString &Url)
{
	World = NewWorld;

	// Exit if the url passed is empty.
	if (Url.IsEmpty())
	{
		return;
	}

	// Check if the SocketClusterContext has already been created other wise exit
	if (LwsContext == nullptr)
	{
		UE_LOG(SocketClusterLog, Error, TEXT("lws_context == nullptr"));
		return;
	}

	// By default we don't connect using SSL
	int UseSSL = 0;

	// Check if we passed the right url format
	int url_find = Url.Find(TEXT(":"));
	if (url_find == INDEX_NONE)
	{
		return;
	}

	// Get the protocol we wan't connect with
	FString protocol = Url.Left(url_find);

	// Check if we passed a supported protocol
	if (protocol.ToLower() != TEXT("ws") && protocol.ToLower() != TEXT("wss"))
	{
		return;
	}

	// Check if we wan't to connect using SSL
	if (protocol.ToLower() == TEXT("wss"))
	{
		UseSSL = 2;
	}

	FString host;
	FString path = TEXT("/");
	FString next_part = Url.Mid(url_find + 3);
	url_find = next_part.Find("/");

	// Check if we passed a path other then /
	// example : ws://example.com/mypath/
	if (url_find != INDEX_NONE)
	{
		host = next_part.Left(url_find);
		path = next_part.Mid(url_find);
	}
	else
	{
		host = next_part;
	}

	// Set address to current host
	FString address = host;

	// Set default port to 80
	int port = 80;

	url_find = address.Find(":");

	// Check if we passed a port to the url
	if (url_find != INDEX_NONE)
	{
		address = host.Left(url_find);
		port = FCString::Atoi(*host.Mid(url_find + 1));
	}
	else
	{
		// If we diden't pass a port and we are using SSL set port to 443
		if (UseSSL)
		{
			port = 443;
		}
	}

	// Create the connection info
	struct lws_client_connect_info info;
	memset(&info, 0, sizeof(info));

	// Convert the FString type to std:string so libwebsockets can read it.
	std::string stdaddress = TCHAR_TO_UTF8(*address);
	std::string stdpath = TCHAR_TO_UTF8(*path);
	std::string stdhost = TCHAR_TO_UTF8(*host);

	// Set connection info
	info.context = LwsContext;
	info.address = stdaddress.c_str();
	info.port = port;
	info.ssl_connection = UseSSL;
	info.path = stdpath.c_str();
	info.host = stdhost.c_str();
	info.origin = stdhost.c_str();
	info.ietf_version_or_minus_one = -1;
	info.userdata = this;

	// Create connection
	Lws = lws_client_connect_via_info(&info);

	// Check if creating connection info was successful
	if (Lws == nullptr)
	{
		UE_LOG(SocketClusterLog, Error, TEXT("Error Trying To Create Client Connecton."));
		return;
	}
}

void USocketCluster::Disconnect()
{
	if (Lws != nullptr)
	{
		/*
		TSharedPtr<FJsonObject> jobj = MakeShareable(new FJsonObject);
		jobj->SetStringField("event", "#disconnect");

		FString jsonstring;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&jsonstring);
		FJsonSerializer::Serialize(jobj.ToSharedRef(), Writer);

		std::string data = TCHAR_TO_UTF8(*jsonstring);
		WsWriteBack(Lws, data.c_str(), -1);

		// lws_close_reason(Lws, LWS_CLOSE_STATUS_NORMAL, NULL, NULL);
		UE_LOG(SocketClusterLog, Error, TEXT("Disconnected"));
		*/
	}
}

// Override Tick Event.
void USocketCluster::Tick(float DeltaTime)
{
	if (LwsContext != nullptr)
	{
		lws_callback_on_writable_all_protocol(LwsContext, &Protocols[0]);
		lws_service(LwsContext, 0);
	}
}

// Override IsTickable Event.
bool USocketCluster::IsTickable() const
{
	// We set Tickable to true to make sure this object is created with tickable enabled.
	return true;
}

// Override GetStatId Event.
TStatId USocketCluster::GetStatId() const
{
	return TStatId();
}

// WebSocket Service Callback Function
int USocketCluster::WsCallback(lws * lws, lws_callback_reasons reason, void * user, void * in, size_t len)
{
	// Get the SocketClusterClient where connection was called from.
	void* pUser = lws_wsi_user(lws);
	USocketCluster* pSocketCluster = (USocketCluster*)pUser;

	// UE_LOG(SocketClusterLog, Error, TEXT("ws_service_callback %i"), reason);

	switch (reason)
	{
		// Connection Established
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
		{
			UE_LOG(SocketClusterLog, Error, TEXT("Connected."));
			TSharedPtr<FJsonObject> jobj = MakeShareable(new FJsonObject);
			jobj->SetStringField("event", "#handshake");
			jobj->SetNumberField("cid", 0);
			jobj->SetObjectField("data", NULL);

			FString jsonstring;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&jsonstring);
			FJsonSerializer::Serialize(jobj.ToSharedRef(), Writer);

			std::string data = TCHAR_TO_UTF8(*jsonstring);
			WsWriteBack(lws, data.c_str(), -1);
		}
		break;

		case LWS_CALLBACK_CLIENT_RECEIVE:
		{
			// UE_LOG(SocketClusterLog, Warning, TEXT("LWS_CALLBACK_CLIENT_RECEIVE"));

			if (strcmp((char *)in, "#1") == 0)
			{
				WsWriteBack(lws, (char*)"#2", -1);
				// UE_LOG(SocketClusterLog, Log, TEXT("Heart Beat"));
			}
			else
			{
				FString recv = UTF8_TO_TCHAR(in);

				UE_LOG(SocketClusterLog, Log, TEXT("Received : %s"), *recv);

				TSharedPtr<FJsonObject> JsonObj;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(*recv);

				if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
				{
					if (JsonObj->HasField("event"))
					{
					}
					else
					{
						int Rid = JsonObj->GetIntegerField("rid");

						if (pSocketCluster->Responses.Contains(Rid))
						{
							TTuple<FResponseCallback, UObject*> Tuple = pSocketCluster->Responses[Rid];

							Tuple.Key.Execute(recv, Tuple.Value);
							pSocketCluster->Responses.Remove(Rid);
						}
					}
				}
			}
		}
		break;

		case LWS_CALLBACK_CLIENT_WRITEABLE:
		{
			if (!pSocketCluster) return -1;
			pSocketCluster->WriteBuffer(lws);
		}
		break;

		default:
			break;

	}

	return 0;
}

int USocketCluster::WsWriteBack(lws * wsi, const char * str, int str_size_in)
{
	if (str == NULL || wsi == NULL)
		return -1;

	int n;
	int len;
	unsigned char *out = NULL;

	if (str_size_in < 1)
		len = strlen(str);
	else
		len = str_size_in;

	out = (unsigned char*)malloc(sizeof(unsigned char)*(LWS_SEND_BUFFER_PRE_PADDING + len + LWS_SEND_BUFFER_POST_PADDING));

	memcpy(out + LWS_SEND_BUFFER_PRE_PADDING, str, len);

	n = lws_write(wsi, out + LWS_SEND_BUFFER_PRE_PADDING, len, LWS_WRITE_TEXT);

	free(out);

	return n;
}

void USocketCluster::WriteBuffer(lws * wsi)
{
	while (Send.Num() > 0)
	{
		FString jsonstring;
		TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&jsonstring);
		FJsonSerializer::Serialize(Send[0].ToSharedRef(), Writer);

		std::string strData = TCHAR_TO_UTF8(*jsonstring);
		WsWriteBack(wsi, strData.c_str(), strData.size());
		Send.RemoveAt(0);
	}
}

// Send Emit Event To SocketCluster Server Function.
void USocketCluster::Emit(const FString& Event, const FString& Data, const FResponseCallback& Callback, UObject* Sender)
{
	// Create a JsonObject To Send
	TSharedPtr<FJsonObject> JObject = MakeShareable(new FJsonObject);
	JObject->SetStringField("event", Event);
	JObject->SetStringField("data", Data);

	// Check if we have a callback event connected
	if (Callback.IsBound())
	{
		JObject->SetNumberField("cid", ++CID);

		Responses.Add(CID, MakeTuple(Callback, Sender));
	}

	// At the json object to the buffer
	Send.Add(JObject);
}
