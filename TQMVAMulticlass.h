//this file looks like plain C, but it's actually -*- c++ -*-
#ifndef __TQMVAMULTICLASS__
#define __TQMVAMULTICLASS__
#include "QFramework/TQNamedTaggable.h"
//#include "QFramework/TQEventObservable.h"
//#idnclude "QFramework/TQObservable.h"
#include "TMVA/Reader.h"
#include "QFramework/TQNamedTaggable.h"

class TQMVAMulticlass : public TQObservable {

public:
  static TQTaggable globalAliases;
  class Reader { //nested
  protected:
    TString fFileName = "";
    TString fMethodName = "";
    TMVA::Reader* fMVAReader = NULL;
    TMVA::IMethod* fMVAMethod = NULL;
    TObjArray* fVariables = NULL;
    mutable std::vector<float> fValues;
    std::vector<TString> fExpressions;
    int parseVariables();
    void assignVariables();
    void clearVariables();
    void printVariables() const;
    void print() const;

  public:
    Reader(const char* filename, const char* methodname);
    size_t size() const;
    const TString& getExpression(size_t i) const;
    virtual void fillValue(size_t i, std::vector<float> val) const ;
 //   std::vector<double>* getVector() const;
    virtual const std::vector<float> getValueMulti() const;
 //   virtual double getValue() const override;
 //   virtual double getValueAt(int index) const;
 //   virtual int getNevaluations() const override;
    TQObservable::ObservableType fType = TQObservable::ObservableType::vector;
 //   inline virtual TQObservable::ObservableType getObservableType() const override {
 //     return fType;
 //   }
    static bool getExpression(TQTaggable* var, TString& result);
  };
protected:
  Reader* fReader = NULL;
  TString fExpression = "";
  std::vector<TQObservable*> fObservables;
  mutable Long64_t fCachedEntry = -1; //!
  mutable std::vector<float> fCachedValue = {-1,-1,-1}; //!
  virtual bool initializeSelf() override;
  virtual bool finalizeSelf() override;

public:
  static TQMVAMulticlass::Reader* getReader(const TString& expression);
  virtual Long64_t getCurrentEntry() const override;
  virtual bool hasExpression() const override;
  virtual const TString& getExpression() const override;
  virtual void setExpression(const TString& expr) override;
  virtual TString getActiveExpression() const override;
  TQMVAMulticlass();
  TQMVAMulticlass(const TString& expression);
  virtual const std::vector<float> getVector() const;
  virtual double getValue() const override;
//  virtual double getValueAt(int index) const;
  virtual TObjArray* getBranchNames() const override;
  virtual ~TQMVAMulticlass();
  DECLARE_OBSERVABLE_FACTORY(TQMVAMulticlass,TString expression)
  ClassDefOverride(TQMVAMulticlass, 0);
};
#endif
