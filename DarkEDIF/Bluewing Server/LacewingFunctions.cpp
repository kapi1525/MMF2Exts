// Handles all Lacewing functions.
#include "Common.h"

#define Ext (*((Extension *) Client.Tag))

void OnError(Lacewing::RelayClient &Client, Lacewing::Error &Error)
{
	SaveExtInfo &S = Ext.AddEvent(0);
	S.Error.Text = _strdup(Error.ToString());

	if (!S.Error.Text)
	{
		Ext.CreateError("Error copying Lacewing error string to local buffer.");
		Ext.Saved.erase(Ext.Saved.end()); // Remove S from vector
	}
}
void OnClientConnect(Lacewing::RelayServer &Server, Lacewing::RelayServer::Client &Client)
{
	Client.IsClosed = false;
	SaveExtInfo &S = Ext.AddEvent(1);
	S.Client = &Client;
}
void OnClientDisconnect(Lacewing::RelayServer &Server, Lacewing::RelayServer::Client &Client)
{
	Client.IsClosed = true;

	SaveExtInfo &S = Ext.AddEvent(3);
	S.Client = &Client;
	S.Channel = NULL;
	
	// Empty all channels and peers
	SaveExtInfo &S = Ext.AddEvent(0xFFFF);
	S.Client = &Client;
}
void OnJoinChannelRequest(Lacewing::RelayServer &Server, Lacewing::RelayServer::Channel &Channel, Lacewing::RelayServer::Client &Client)
{
	Channel.IsClosed = false;
	
	for (Lacewing::RelayClient::Channel::Peer * i = Target.FirstPeer(); i != nullptr; i = i->Next())
		i->IsClosed = false;

	SaveExtInfo &S = Ext.AddEvent(4);
	S.Channel = &Channel;
}
void OnLeaveChannelRequest(Lacewing::RelayServer &Server, Lacewing::RelayServer::Client &Client, Lacewing::RelayServer::Channel &Channel)
{
	SaveExtInfo &S = Ext.AddEvent(X);
	S.Channel = &Channel;
	S.Client = &Client;
	
	// Clear channel copy after this event is handled
	SaveExtInfo &C = Ext.AddEvent(0xFFFF);
	C.Channel = S.Channel;
}
void OnNameSetRequest(Lacewing::RelayClient &Client)
{
	SaveExtInfo &S = Ext.AddEvent(6);
	S.Client = &Client;
	S.RequestedName = ;
}
void OnPeerConnect(Lacewing::RelayClient &Client, Lacewing::RelayClient::Channel &Channel, Lacewing::RelayClient::Channel::Peer &Peer)
{
	Peer.IsClosed = false;

	SaveExtInfo &S = Ext.AddEvent(10);
	S.Channel = &Channel;
	S.Peer = &Peer;
}
void OnPeerDisconnect(Lacewing::RelayClient &Client, Lacewing::RelayClient::Channel &Channel, Lacewing::RelayClient::Channel::Peer &Peer)
{
	Peer.IsClosed = true;

	SaveExtInfo &S = Ext.AddEvent(11);
	S.Channel = &Channel;
	S.Peer = &Peer;
	
	// Remove closed peer entirely after above event is handled
	SaveExtInfo &C = Ext.AddEvent(0xFFFF);
	C.Channel = S.Channel;
	C.Peer = S.Peer;
}
void OnPeerNameChanged(Lacewing::RelayClient &Client, Lacewing::RelayClient::Channel &Channel, Lacewing::RelayClient::Channel::Peer &Peer, const char * OldName)
{
	SaveExtInfo &S = Ext.AddEvent(45);
	S.Channel = &Channel;
	S.Peer = &Peer;

	// Old previous name? Free it
	if (Peer.Tag)
		free(Peer.Tag);

	// Store new previous name in Tag.
	Peer.Tag = _strdup(OldName);
	if (!Peer.Tag)
		Ext.CreateError("Error copying old peer name.");
}
void OnPeerMessage(Lacewing::RelayClient &Client, Lacewing::RelayClient::Channel &Channel, Lacewing::RelayClient::Channel::Peer &Peer,
				   bool Blasted, int Subchannel, char * Data, int Size, int Variant)
{
	SaveExtInfo &S = Ext.AddEvent(Blasted ? 52 : 49);
	S.Channel = &Channel;
	S.Peer = &Peer;
	S.ReceivedMsg.Subchannel = (unsigned char) Subchannel;
	S.ReceivedMsg.Size = Size;

	S.ReceivedMsg.Content = (char *)malloc(Size);
	if (!S.ReceivedMsg.Content)
	{
		Ext.CreateError("Failed to create local buffer for copying received message from Lacewing. Message discarded.");
	}
	else if (memcpy_s(S.ReceivedMsg.Content, Size, Data, Size))
	{
		Ext.Saved.erase(Ext.Saved.end());
		free(S.ReceivedMsg.Content);
		Ext.CreateError("Failed to copy message from Lacewing to local buffer. Message discarded.");
	}
	else 
	{
		if (Blasted)
		{
			// Text
			if (Variant == 0)
				Ext.AddEvent(39, true);
			// Number
			else if (Variant == 1)
				Ext.AddEvent(40, true);
			// Binary
			else if (Variant == 2)
				Ext.AddEvent(41, true);
			// ???
			else
				Ext.CreateError("Warning: message type is neither binary, number nor text.");
		}
		else // Sent
		{
			// Text
			if (Variant == 0)
				Ext.AddEvent(36, true);
			// Number
			else if (Variant == 1)
				Ext.AddEvent(37, true);
			// Binary
			else if (Variant == 2)
				Ext.AddEvent(38, true);
			// ???
			else
				Ext.CreateError("Warning: message type is neither binary, number nor text.");
		}
	}
}
void OnChannelMessage(Lacewing::RelayClient &Client, Lacewing::RelayClient::Channel &Channel, Lacewing::RelayClient::Channel::Peer &Peer,
					  bool Blasted, int Subchannel, char * Data, int Size, int Variant)
{
	SaveExtInfo &S = Ext.AddEvent(Blasted ? 51 : 48);
	S.Channel = &Channel;
	S.Peer = &Peer;
	S.ReceivedMsg.Subchannel = (unsigned char)Subchannel;
	S.ReceivedMsg.Size = Size;

	S.ReceivedMsg.Content = (char *)malloc(Size);
	if (!S.ReceivedMsg.Content)
	{
		Ext.CreateError("Failed to create local buffer for copying received message from Lacewing. Message discarded.");
	}
	else if (memcpy_s(S.ReceivedMsg.Content, Size, Data, Size))
	{
		Ext.Saved.erase(Ext.Saved.end());
		free(S.ReceivedMsg.Content);
		Ext.CreateError("Failed to copy message from Lacewing to local buffer. Message discarded.");
	}
	else
	{
		if (Blasted)
		{
			// Text
			if (Variant == 0)
				Ext.AddEvent(22, true);
			// Number
			else if (Variant == 1)
				Ext.AddEvent(23, true);
			// Binary
			else if (Variant == 2)
				Ext.AddEvent(35, true);
			// ???
			else
				Ext.CreateError("Warning: message type is neither binary, number nor text.");
		}
		else // Sent
		{
			// Text
			if (Variant == 0)
				Ext.AddEvent(9, true);
			// Number
			else if (Variant == 1)
				Ext.AddEvent(16, true);
			// Binary
			else if (Variant == 2)
				Ext.AddEvent(33, true);
			// ???
			else
				Ext.CreateError("Warning: message type is neither binary, number nor text.");
		}
	}
}
void OnServerMessage(Lacewing::RelayClient &Client,
					 bool Blasted, int Subchannel, char * Data, int Size, int Variant)
{
	SaveExtInfo &S = Ext.AddEvent(Blasted ? 50 : 47);
	S.ReceivedMsg.Subchannel = (unsigned char)Subchannel;
	S.ReceivedMsg.Size = Size; // Do NOT add null. There's error checking for that.

	S.ReceivedMsg.Content = (char *)malloc(Size);

	if (!S.ReceivedMsg.Content)
	{
		Ext.CreateError("Failed to create local buffer for copying received message from Lacewing. Message discarded.");
	}
	else if (memcpy_s(S.ReceivedMsg.Content, Size, Data, Size))
	{
		Ext.Saved.erase(Ext.Saved.end());
		free(S.ReceivedMsg.Content);
		Ext.CreateError("Failed to copy message from Lacewing to local buffer. Message discarded.");
	}
	else
	{
		if (Blasted)
		{
			// Text
			if (Variant == 0)
				Ext.AddEvent(20, true);
			// Number
			else if (Variant == 1)
				Ext.AddEvent(21, true);
			// Binary
			else if (Variant == 2)
				Ext.AddEvent(34, true);
			// ???
			else
				Ext.CreateError("Warning: message type is neither binary, number nor text.");
		}
		else // Sent
		{
			// Text
			if (Variant == 0)
				Ext.AddEvent(8, true);
			// Number
			else if (Variant == 1)
				Ext.AddEvent(15, true);
			// Binary
			else if (Variant == 2)
				Ext.AddEvent(32, true);
			// ???
			else
				Ext.CreateError("Warning: message type is neither binary, number nor text.");
		}
	}
}
void OnServerChannelMessage(Lacewing::RelayClient &Client, Lacewing::RelayClient::Channel &Channel,
							bool Blasted, int Subchannel, char * Data, int Size, int Variant)
{
	SaveExtInfo &S = Ext.AddEvent(Blasted ? 72 : 68);
	S.Channel = &Channel;
	S.ReceivedMsg.Subchannel = (unsigned char) Subchannel;
	S.ReceivedMsg.Size = Size;

	S.ReceivedMsg.Content = (char *)malloc(Size);
	if (!S.ReceivedMsg.Content)
	{
		Ext.CreateError("Failed to create local buffer for copying received message from Lacewing. Message discarded.");
	}
	else if (memcpy_s(S.ReceivedMsg.Content, Size, Data, Size))
	{
		Ext.Saved.erase(Ext.Saved.end());
		free(S.ReceivedMsg.Content);
		Ext.CreateError("Failed to copy message from Lacewing to local buffer. Message discarded.");
	}
	else 
	{
		if (Blasted)
		{
			// Text
			if (Variant == 0)
				Ext.AddEvent(69, true);
			// Number
			else if (Variant == 1)
				Ext.AddEvent(70, true);
			// Binary
			else if (Variant == 2)
				Ext.AddEvent(71, true);
			// ???
			else
				Ext.CreateError("Warning: message type is neither binary, number nor text.");
		}
		else // Sent
		{
			// Text
			if (Variant == 0)
				Ext.AddEvent(65, true);
			// Number
			else if (Variant == 1)
				Ext.AddEvent(66, true);
			// Binary
			else if (Variant == 2)
				Ext.AddEvent(67, true);
			// ???
			else
				Ext.CreateError("Warning: message type is neither binary, number nor text.");
		}
	}
}


#undef Ext