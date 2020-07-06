void test(TString fileName = "1594031885.root")
{
   auto file = new TFile(fileName, "READ");
   auto tree = (TTree*) file->Get("data");
   tree->Print();

   UChar_t mod;
   tree->SetBranchAddress("Mod", &mod);

   UChar_t ch;
   tree->SetBranchAddress("Ch", &ch);

   ULong64_t timeStamp;
   tree->SetBranchAddress("TimeStamp", &timeStamp);



   const auto nData = tree->GetEntries();
   for(auto iData = 0; iData < nData; iData++) {
      tree->GetEntry(iData);

      cout << Int_t(mod) <<"\t"<< Int_t(ch) <<"\t"<< timeStamp << endl;
   }
   
}
