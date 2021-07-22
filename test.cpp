void test()
{
   auto fileName = "result.root";
   auto file = new TFile(fileName, "READ");
   auto tree = (TTree*)file->Get("FineTS");
   Int_t ch;
   tree->SetBranchAddress("Ch", &ch);
   Int_t brd;
   tree->SetBranchAddress("Mod", &brd);
   ULong64_t timeStamp;
   tree->SetBranchAddress("Time", &timeStamp);
   
   std::vector<ULong64_t> ts[6];
   
   const auto nHits = tree->GetEntries();
   for(auto i = 0; i < nHits; i++){
      tree->GetEntry(i);

      if(ch == 1) {
         ts[brd].push_back(timeStamp);
      }
   }

   auto minSize = std::min({ts[0].size(), ts[1].size(), ts[2].size(),
                            ts[3].size(), ts[4].size(), ts[5].size()});
   
   for(auto i = 0; i < minSize; i++){
      //for(auto i = 0; i < 100; i++){
      Double_t ts0, ts1, ts2, ts3, ts4, ts5;
      ts0 = ts[0][i];
      ts1 = ts[1][i];
      ts2 = ts[2][i];
      ts3 = ts[3][i];
      ts4 = ts[4][i];
      ts5 = ts[5][i];
      std::cout << ts0 - ts1 <<"\t"
                << ts1 - ts2 <<"\t"
                << ts2 - ts3 <<"\t"
                << ts3 - ts4 <<"\t"
                << ts4 - ts5 << std::endl; 
   }
}
