#include "QFramework/TQStringUtils.h"
#include "QFramework/TQNamedTaggable.h"
#include "QFramework/TQIterator.h"
#include "QFramework/TQPathManager.h"
#include "QFramework/TQUtils.h"

// #define _DEBUG_
#include "QFramework/TQLibrary.h"

#include "TMVA/IMethod.h"
#include "QFramework/TQMVAMulticlass.h"
#include <limits>


////////////////////////////////////////////////////////////////////////////////////////////////
//
// TQMVAMulti
//
// The TQMVAMulti is a variant of TQObservable that is able to
// take the location of an MVA XML file and instantiate a TMVA::Reader
// base on its settings. The result of the observable will be the
// evaluation of the classifier defined by the XML file.
//
////////////////////////////////////////////////////////////////////////////////////////////////

/*@observable: [TQMVAMulti] The TQMVAObservable provides a way to evaluate TMVA classifiers.
    Similar to the TQMultiObservable it relies fully on its subObservables which it evaluates in 
    order to forward their values to the (trained) MVA. It then provides the reponse of the MVA.
    It is automatically instantiated when encountering an expression either matching '*weights.xml*'
    or starting with 'MVA:'. In the latter case the 'MVA:' part is removed and in either case
    the remaining expression is interpreted as the path to the MVA weights file which is then 
    passed to TMVA (after passing it through the TQPathManager, i.e., in CAFExample based analysis 
    packages, paths can simply be provided relative to (typically) the 'share' directory just like
    non-absolute paths to configuration files. 
    Additionally, this observable requires the weights xml file to be slightly
    modified compared to those produced by default by TMVA: the 'Label' attribute of each variable
    in the xml must contain the Observable expression corresponding to the variable. The 
    subObservables of the TQMVAMulti are then automatically retrieved using these expressions.
    

*/

ClassImp(TQMVAMulticlass)

#ifndef NO_LIBXML2
#include <libxml/tree.h>
#include <libxml/parserInternals.h>
#endif

TQTaggable TQMVAMulticlass::globalAliases = TQTaggable();

//______________________________________________________________________________________________

TQMVAMulticlass::TQMVAMulticlass(){
  // default constructor
  this->SetName("TQMVAMulticlass");
}

//______________________________________________________________________________________________

TQMVAMulticlass::TQMVAMulticlass(const TString& expression){
  // constructor with an expression
  DEBUGclass("constructor called with expression '%s'",expression.Data());
  this->setExpression(expression);
  TString expr(expression);
  this->SetName(TQFolder::getPathTail(expr));
}

//______________________________________________________________________________________________

TQMVAMulticlass::~TQMVAMulticlass(){
  // destructor
}

//______________________________________________________________________________________________

void TQMVAMulticlass::setExpression(const TString& expr){
  // set the expression to a given string
  this->fExpression = TQStringUtils::compactify(expr);
}

//______________________________________________________________________________________________

#ifndef NO_LIBXML2
namespace{
  xmlNodePtr findNodeByName(xmlNodePtr rootnode, const char * nodename){
    xmlNodePtr node = rootnode;
    if(node == NULL){
      TQLibrary::ERRORfunc("unable to retrieve node '%s' - document empty!", nodename);
      return NULL;
    }
    while(node != NULL){
      if(!xmlStrcmp(node->name, (const xmlChar*)(nodename))){
        return node; 
      } else if(node->children){
        xmlNodePtr intNode = findNodeByName(node->children, nodename); 
        if(intNode) return intNode;
      }
      node = node->next;
    }
    return NULL;
  }

  //______________________________________________________________________________________________

  bool findAttributeValue(xmlNodePtr node, const char* attrname, TString& val){
    if(node == NULL) return false;
    xmlAttr* attribute = node->properties;
    while(attribute && attribute->name && attribute->children){
      if(!xmlStrcmp(attribute->name, (const xmlChar*)(attrname))){
        xmlChar* value = xmlNodeListGetString(node->doc, attribute->children, 1);
        val = TString((const char*)value);
        free(value); 
        return true;
      }
      attribute = attribute->next;
    }
    return false;
  }

  //______________________________________________________________________________________________

  bool findNodeAttributeMatch(xmlNodePtr node, const char* attrname, const TString& attrval){
    if(node == NULL) return false;
    xmlNodePtr child = node->children;
    TString val;
    while(child != NULL){
      if(findAttributeValue(child,attrname,val)){
        if(TQStringUtils::matches(val,attrval))
          return true;
      }
      child = child->next;
    }
    return false;
  }

  //______________________________________________________________________________________________

  xmlNodePtr findNodeByNameAndAttribute(xmlNodePtr rootnode, const char* nodename, const TString& attrname, const TString& attrval){
    xmlNodePtr node = rootnode;
    if(node == NULL){
      TQLibrary::ERRORfunc("unable to retrieve node '%s' - document empty!", nodename);
      return NULL;
    }
 
    while(node != NULL){
      if(!xmlStrcmp(node->name, (const xmlChar*)(nodename))){
        return node; 
      } else if(node->children){
        xmlNodePtr intNode = findNodeByNameAndAttribute(node->children, nodename,attrname,attrval); 
        if(findNodeAttributeMatch(intNode,attrname,attrval)) return intNode;
      }
      node = node->next;
    }
    return NULL;
  }
}
#endif

//______________________________________________________________________________________________

bool TQMVAMulticlass::Reader::getExpression(TQTaggable* var, TString& result) {
  if(!var) return false;
  TString expr = var->getTagStringDefault("Expression","");
  TString name = var->getTagStringDefault("Internal","");
  TString label = var->getTagStringDefault("Label","");
  if(label.IsNull()) return false;
  if(expr.IsNull()) return false;
  if(name.IsNull()) return false;
  if(expr == name && label == name){ result = name; return true; }
  else if (expr == name){ result = label; return true; }
  else if (label == name){ result = expr; return true; }
  return false;
}

//______________________________________________________________________________________________

TQMVAMulticlass::Reader::Reader(const char* filename, const char* methodname) : 
  fFileName(filename),
  fMethodName(methodname),
  fVariables(new TObjArray())
{
  // perform the entire setup from the given file name
  this->fMVAReader = new TMVA::Reader("Silent",false);
  this->clearVariables();
  if(this->parseVariables() > 0){
    this->assignVariables();
    this->fMVAMethod = this->fMVAReader->BookMVA(this->fMethodName, this->fFileName);
    if(!this->fMVAMethod){
      throw std::runtime_error(TString::Format("unable to book method '%s' in MVA::Reader from file '%s'",this->fMethodName.Data(),this->fFileName.Data()).Data());
    }
  } else {
    throw std::runtime_error(TString::Format("no variables found in file '%s'",this->fFileName.Data()).Data());
  }
}

//______________________________________________________________________________________________

void TQMVAMulticlass::Reader::printVariables() const {
  // print the variables of this observable
  TQTaggableIterator itr(fVariables);
  std::cout << TQStringUtils::makeBoldWhite("Variables") << " of " << TQStringUtils::makeBoldWhite(this->fMethodName) << " as read from " << TQStringUtils::makeBoldWhite(this->fFileName) << std::endl;
  while(itr.hasNext()){
    TQTaggable* var = itr.readNext();
    if(!var) continue;
    std::cout << var->exportTagsAsString() << std::endl;
  }
}

//______________________________________________________________________________________________

void TQMVAMulticlass::Reader::clearVariables(){
  // clear the variables of this observable
  this->fVariables->Clear();
  this->fValues.clear();
}

//______________________________________________________________________________________________

int TQMVAMulticlass::Reader::parseVariables(){
#ifdef NO_LIBXML2
  ERRORclass("libxml2 support is disabled - unable to parse variables!");
#warning "compiling without libxml2 support!"
  return 0;
#else
  // parse the variables from this input file
  DEBUG("parsing variables from file '%s'",this->fFileName.Data());
  TString it=TQStringUtils::readFile(this->fFileName);
  if(it.IsNull()){
    ERRORclass("unable to open file '%s'",this->fFileName.Data());
    return -1;
  }
  xmlDocPtr xmldoc_ptr;
  xmlParserCtxtPtr ctxt_ptr = xmlNewParserCtxt();
  if(ctxt_ptr == NULL) return -1;
  xmldoc_ptr = xmlCtxtReadMemory( ctxt_ptr, it.Data(), it.Length(), this->fFileName.Data(), "ISO-8859-1", 0);
  if(xmldoc_ptr == NULL) return -1;
  xmlNodePtr root_element_ptr = xmlDocGetRootElement(xmldoc_ptr);
  xmlNodePtr basenode = root_element_ptr;
  if(this->fMethodName.IsNull()){
    basenode = findNodeByName(root_element_ptr,"MethodSetup");
    TString buffer;
    findAttributeValue(basenode,"Method",fMethodName);
    TQStringUtils::readUpToText(fMethodName,buffer,"::");
    TQStringUtils::removeLeadingText(fMethodName,"::");
  } else {
    TString methodname(fMethodName);
    basenode = findNodeByNameAndAttribute(root_element_ptr,"MethodSetup","Method",methodname);
  }
 
  xmlNodePtr variables = findNodeByName(basenode,"Variables");
  xmlNodePtr node = variables->children;
  while(node != NULL){
    if(!xmlStrcmp(node->name, (const xmlChar*)("Variable"))){
      DEBUG("looking at variable");
      TQNamedTaggable* var = new TQNamedTaggable("variable");
      xmlAttr* attribute = node->properties;
      while(attribute && attribute->name && attribute->children){
        xmlChar* value = xmlNodeListGetString(node->doc, attribute->children, 1);
        var->setTagString((const char*)attribute->name,(const char*)value);
        free(value); 
        attribute = attribute->next;
      }
      DEBUG("finished variable '%s'",var->getTagStringDefault("Internal","").Data());
      TString expression;
      if(this->getExpression(var,expression)){
        this->fVariables->Add(var);
        this->fValues.push_back(0.);
        TString expr = var->getTagStringDefault("Label",expression);
        this->fExpressions.push_back(TQMVAMulticlass::globalAliases.replaceInTextRecursive(expr));
      } else {
        WARNclass("unable to parse variable from XML: %s",var->exportTagsAsString().Data());
        delete var;
      }
    }
    node = node->next;
  }
  return fValues.size();
#endif
}

//______________________________________________________________________________________________

void TQMVAMulticlass::Reader::print() const {
  // print a general overview over the current configuration
  std::cout << TQStringUtils::makeBoldBlue("TQMVAMulticlass::Reader");
  std::cout << "\t" << TQStringUtils::fixedWidth("fMethodName",20,"l") << " = " << this->fMethodName << std::endl;
  std::cout << "\t" << TQStringUtils::fixedWidth("fFileName",20,"l") << " = " << this->fFileName << std::endl;
  this->printVariables();
}

//______________________________________________________________________________________________

void TQMVAMulticlass::Reader::assignVariables(){
  // assign the variables of this observable to the internal MVA reader
  TQTaggableIterator itr(fVariables);
  while(itr.hasNext()){
    TQTaggable* var = itr.readNext();
    if(!var) continue;
    TString varname;
    if(var->getTagString("Internal",varname)){
      fMVAReader->AddVariable(varname, &(this->fValues[itr.getLastIndex()]));
    }
  }
}



size_t TQMVAMulticlass::Reader::size() const {
  return this->fValues.size();
}
const TString& TQMVAMulticlass::Reader::getExpression(size_t i) const {
  return this->fExpressions[i];
}

const std::vector<float> TQMVAMulticlass::Reader::getValueMulti() const {
  DEBUGclass("calculated weight for reader %s: %g",this->fFileName.Data(),this->fMVAMethod->EvaluateMulticlass("BDTG method"));
	std::cout << "multiclassifier running!" << std::endl;
  	return this->fMVAReader->EvaluateMulticlass("BDTG method");
}

void TQMVAMulticlass::Reader::fillValue(size_t i, std::vector<float> val) const{
	this->fValues[i]=val[i];
}


//______________________________________________________________________________________________

const std::vector<float> TQMVAMulticlass::getVector() const {
  // Function to retrieve the cached vector
  // retrieve the value of this observable
  #ifdef _DEBUG_
  if(!this->fReader){
    throw std::runtime_error("Reader is NULL");
  }
  #endif
  if(this->getCurrentEntry() != this->fCachedEntry){
    DEBUGclass("this event '%d'",this->getCurrentEntry());
    for(size_t i=0; i<this->fReader->size(); ++i){
      const std::vector<float> value = this->fReader->getValueMulti();
      this->fReader->fillValue(i,value);
      DEBUGclass("filling value[%d]=%g (from '%s')",(int)(i),value,this->fReader[i]->getActiveExpression().Data());
    }
    this->fCachedValue = this->fReader->getValueMulti();
    this->fCachedEntry = this->getCurrentEntry();
  } else {
    DEBUGclass("re-using cached value from event '%d'",this->fCachedEntry);
  }
  return this->fCachedValue;
}

//____________________________________________________________________________________________

double TQMVAMulticlass::getValue() const {
  // Value retrieval function, called on every event for every cut and histogram
  // Since this is a vector observable this should never be executed
  DEBUGclass("entering function getValue");
  throw std::runtime_error("Caught attempt to evaluate vector valued observable in scalar context");
  return std::numeric_limits<float>::quiet_NaN();
}

//______________________________________________________________________________________________
/*
double TQMVAMulticlass::getValueAt(int index) const {
  // Function to return element of vector with specified index
  const std::vector<float> vec = this->getVector();

  if ((int)vec->size() == 0) { // vector is empty, not even filled once
    throw std::runtime_error("Attempted to retrieve value of vector that is empty");
    return std::numeric_limits<float>::quiet_NaN();
  }
  if (index >= (int)vec->size()) {
    throw std::runtime_error("Attempted to retrieve value for out of bounds");
    return std::numeric_limits<float>::quiet_NaN();
  }
  return vec->at(index);
}
*/
//______________________________________________________________________________________________

TString TQMVAMulticlass::getActiveExpression() const {
  // return the currently active expression
  return this->fExpression;
}

//______________________________________________________________________________________________

TObjArray* TQMVAMulticlass::getBranchNames() const {
  // retrieve the branch names associated to this observable
  TObjArray* arr = new TObjArray();
  for(size_t i=0; i<this->fObservables.size(); ++i){
    TQObservable* obs = this->fObservables[i];
    if(!obs) continue;
    TCollection* branches = this->fObservables[i]->getBranchNames();
    if(branches){
      arr->AddAll(branches);
      delete branches;
    }
  }
  return arr;
}

//______________________________________________________________________________________________

TQMVAMulticlass::Reader* TQMVAMulticlass::getReader(const TString& expression){
  static std::map<const TString, TQMVAMulticlass::Reader*> sReaders;
  auto it = sReaders.find(expression);
  if(it == sReaders.end()){
    TString filename,methodname;
    TQFolder::parseLocation(expression,filename,methodname);
    filename = TQPathManager::findFileFromEnvVar(filename, "CAFANALYSISSHARE");
    DEBUG("creating new TQMVAMulticlass::Reader from '%s' with '%s':'%s'",expression.Data(),filename.Data(),methodname.Data());
    TQMVAMulticlass::Reader* rd = new TQMVAMulticlass::Reader(filename,methodname);
    sReaders[expression] = rd;
    return rd;
  } else {
    DEBUG("reusing TQMVAMulticlass::Reader with expression '%s'",expression.Data());
    return it->second;
  }
}

//______________________________________________________________________________________________

bool TQMVAMulticlass::initializeSelf(){
  // initialize this observable on the current sample
  if(this->fReader) return true;
  bool retval = true;

  TString filename, methodname;
  this->fReader = this->getReader(this->fExpression);
  if(!fReader) throw std::runtime_error(TString::Format("in TQMVAMulticlass: unable to obtain reader for expression '%s'",this->fExpression.Data()).Data());

  this->fCachedEntry = -1;
  for(size_t i=0; i<this->fReader->size(); ++i){
    TString expression(this->fReader->getExpression(i));
    TQObservable* obs = TQObservable::getObservable(expression,this->fSample);
    this->fObservables.push_back(obs);
    if(!obs->initialize(this->fSample)) {
      retval = false;
      ERRORclass("Failed to initialize sub-observable obtained from expression '%s' in TQMVAMulticlass for sample '%s'",this->fReader->getExpression(i).Data(),this->fSample->getPath().Data());
    }
  }
  return retval;
}

//______________________________________________________________________________________________

bool TQMVAMulticlass::finalizeSelf(){
  // finalize this observable on the current sample
  bool retval = true;
  for(size_t i=0; i<this->fObservables.size(); i++){
    if(!this->fObservables[i]->finalize()) retval = false;
  }
  this->fReader = NULL;
  this->fObservables.clear();
  return retval;
}

//______________________________________________________________________________________________

Long64_t TQMVAMulticlass::getCurrentEntry() const {
  // retrieve the current entry from the tree
  if(this->fObservables.size() == 0) return -1;
  for (size_t i = 0; i<this->fObservables.size(); i++) {
    if (this->fObservables[i]->getCurrentEntry() >= 0) return this->fObservables[i]->getCurrentEntry();
  }
  return -1;
}

//______________________________________________________________________________________________

const TString& TQMVAMulticlass::getExpression() const {
  // retrieve the expression associated with this observable
  return this->fExpression;
}

//______________________________________________________________________________________________

bool TQMVAMulticlass::hasExpression() const {
  // check if this observable type knows expressions (default false)
  return true;
}

//______________________________________________________________________________________________

DEFINE_OBSERVABLE_FACTORY(TQMVAMulticlass,TString expression){
  // try to create an instance of this observable from the given expression
  if(TQStringUtils::matches(expression,"*_BDTG.weightsMulti.xml*")){
    return new TQMVAMulticlass(expression);
  }
  return NULL;
}

