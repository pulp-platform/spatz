Commit description:
f17185b576ea10dd173ab41f0856b85040caead1
1. The spatz_cc datapath is fully 39-bit ( VRF<-> TCDM)
2. The VLSU is implemented in only tail-agnositic mode.
  Namely, no RMW FSM is implemented in VLSU.
Reasoning:
RMW would only become necessary if Spatz ever adds mask-undisturbed support.