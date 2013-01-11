#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/dce-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/dce-dnssec-module.h"
#include <fstream>

using namespace ns3;

static void RunIp (Ptr<Node> node, Time at, std::string str)
{
  DceApplicationHelper process;
  ApplicationContainer apps;
  process.SetBinary ("ip");
  process.SetStackSize (1<<16);
  process.ResetArguments();
  process.ParseArguments(str.c_str ());
  apps = process.Install (node);
  apps.Start (at);
}

static void AddAddress (Ptr<Node> node, Time at, const char *name, const std::string prefixAddr,
                        int number, std::string suffixAddr)
{
  std::ostringstream oss;
  oss << "-f inet addr add " << prefixAddr << number << suffixAddr << " dev " << name;
  RunIp (node, at, oss.str ());
}

bool m_delay = true;
bool nNodes = 1;

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.AddValue ("delay", "add process delay (default 1)", m_delay);
  cmd.AddValue ("nNodes", "the number of client nodes (default 1)", nNodes);
  cmd.Parse (argc, argv);

  NodeContainer trustAuth, subAuth, fakeRoot, cacheSv, client;
  NodeContainer nodes;
  fakeRoot.Create (1);
  trustAuth.Create (1);
  subAuth.Create (1);
  cacheSv.Create (1);
  client.Create (nNodes);
  nodes = NodeContainer (trustAuth, subAuth, fakeRoot, cacheSv, client);

  NetDeviceContainer devices;

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("500Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("1ms"));
  devices = csma.Install (nodes);
/*
  Ptr<RateErrorModel> em = CreateObjectWithAttributes<RateErrorModel> (
                           "RanVar", RandomVariableValue (UniformVariable (0., 1.)),
                           "ErrorRate", DoubleValue (0.00001));
  devices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
*/
  csma.EnablePcapAll ("process-dnssec");

  DceManagerHelper processManager;
  //processManager.SetLoader ("ns3::DlmLoaderFactory");
  if (m_delay)
    {
#if 0
      processManager.SetDelayModel ("ns3::TimeOfDayProcessDelayModel");
#endif
    }
  processManager.SetTaskManagerAttribute ("FiberManagerType",
                                          EnumValue (0));
  processManager.SetNetworkStack("ns3::LinuxSocketFdFactory",
				 "Library", StringValue ("libnet-next-2.6.so"));
  processManager.Install (nodes);

  for (int n=0; n < nodes.GetN (); n++)
    {
      AddAddress (nodes.Get (n), Seconds (0.1), "sim0", "10.0.0.", 1 + n, "/8" );
      RunIp (nodes.Get (n), Seconds (0.2), "link set sim0 up");
      // RunIp (nodes.Get (n), Seconds (0.2), "link show");
      // RunIp (nodes.Get (n), Seconds (0.3), "route show table all");
      // RunIp (nodes.Get (n), Seconds (0.4), "addr list");
    }

  DceApplicationHelper process;
  ApplicationContainer apps;
  Bind9Helper bind9;
  UnboundHelper unbound;
  // 
  // FakeRoot Configuration (node 0)
  // 
  bind9.UseManualConfig (fakeRoot);
  bind9.Install (fakeRoot);

  // 
  // Tursted Anthor Authority Server Configuration (node 1)
  // 
  bind9.UseManualConfig (trustAuth);
  bind9.Install (trustAuth);

  // 
  // Authority Server of Sub-Domain Configuration (node 2)
  // 
  bind9.UseManualConfig (subAuth);
  bind9.Install (subAuth);

  // 
  // Cache Server Configuration (node 3)
  // 
  bind9.UseManualConfig (cacheSv);
  bind9.Install (cacheSv);
  // process.SetBinary ("unbound");
  // process.ResetArguments ();
  // process.SetStackSize (1<<16);
  // process.Install (cacheSv);

  // 
  // Client Configuration (node 4 - n)
  // 
  for (int i = 0; i < nNodes; i++)
    {
      // node3 is forwarder
      unbound.SetForwarder (client.Get (i), "10.0.0.4");
      for (int j = 0; j < 20; j++)
	{
	  unbound.SendQuery (client.Get (i), Seconds (1+ 10*j), 
			     "ns.example.org");
	}
      unbound.Install (client.Get (i));

    }


  Simulator::Stop (Seconds (2000000.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
