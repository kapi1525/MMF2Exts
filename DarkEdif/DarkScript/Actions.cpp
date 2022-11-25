#include "Common.h"

void Extension::Template_SetFuncSignature(const TCHAR * funcSig, int delayable, int repeatable, int recursable)
{
	if (funcSig[0] == _T('\0'))
		return CreateErrorT("%s: You must supply a function signature, not blank.", funcSig);

	// Test these first cos they're faster
	if ((recursable & 1) != recursable)
		return CreateErrorT("%s: Parameter recursable must be 0 or 1, you supplied %i.", _T(__FUNCTION__), recursable);
	if (delayable < 0 || delayable > 2)
		return CreateErrorT("%s: Parameter \"delay expected\" must be 0, 1 or 2, you supplied %i.", _T(__FUNCTION__), delayable);
	if (repeatable < 0 || repeatable > 2)
		return CreateErrorT("%s: Parameter \"repeating expected\" must be 0, 1 or 2, you supplied %i.", _T(__FUNCTION__), repeatable);

	// Complicated escaping here; to convert regex to C++ string, take original regex, double the backslashes, and escape double quotes with one backslash.
	// Original regex:
	// (any|int|string|float)\s+([^\s(]+)\s*\(((?:\s*(?:[^\s]+)\s+(?:[^,\s]+)(?:\s*=\s*(?:[^",]+|(?:"(?:(?=(?:\\?)).)+?")))?,\s*)*(?:\s*(?:[^\s]+)\s+(?:[^,)\s=]+)(?:\s*=\s*(?:[^",]+|(?:"(?:(?=(?:\\?)).)+?")))?)?)\)
	//
	// This regex takes into account strings with backslash escaping in default values, double quotes, etc.
	// Anything is allowed for parameter names, as long as it's 1+ character, does not contain whitespace or brackets, and is unique.
	// So, to confuse yourself, {} and [] are allowed as parameter names.
	const std::basic_regex<TCHAR> funcSigParser(_T("(any|int|string|float)\\s+([^\\s(]+)\\s*\\(((?:\\s*(?:[^\\s]+)\\s+(?:[^,\\s]+)(?:\\s*=\\s*(?:[^\",]+|(?:\"(?:(?=(?:\\\\?)).)+?\")))?,\\s*)*(?:\\s*(?:[^\\s]+)\\s+(?:[^,)\\s=]+)(?:\\s*=\\s*(?:[^\",]+|(?:\"(?:(?=(?:\\\\?)).)+?\")))?)?)\\)"s));

	std::match_results<std::tstring::const_iterator> funcSigBreakdown;
	const std::tstring funcSigStr(funcSig);

	if (!std::regex_match(funcSigStr.cbegin(), funcSigStr.cend(), funcSigBreakdown, funcSigParser))
		return CreateErrorT("%s: Function signature \"%s\" not parseable.", _T(__FUNCTION__), funcSig);

	const std::tstring funcNameL(ToLower(funcSigBreakdown[2].str()));

	// Make sure it's not a KRFuncXXX() function name. That would work, but the script engine
	// (used in "run text as script") will find it ambiguous.
	const std::basic_regex<TCHAR> isKRFunc(_T("k?r?f?func[fis]*\\$?"s));
	if (std::regex_match(funcNameL, isKRFunc))
		return CreateErrorT("%s: Function name \"%s\" is invalid; KRFuncXX format will confuse the script parser.", _T(__FUNCTION__), funcSigBreakdown[2].str().c_str());

	Type returnTypeValid;
	if (!StringToType(returnTypeValid, funcSigBreakdown[1].str().c_str()))
		return CreateErrorT("%s: Return type \"%s\" not recognised. Use Any, String, Integer, or Float.", _T(__FUNCTION__), funcSigBreakdown[1].str().c_str());

	std::vector<Param> params;

	// do parameters exist?
	if (funcSigBreakdown[3].length() != 0)
	{
		// Regex is dumb and for repeating groups, only the last one will be considered;
		// so we re-run a regex on the parameter list, looping all matches.
		// .NET regex does capture repeating groups nicely, but no other regex engines support it.
		const std::basic_regex<TCHAR> paramListParser(_T("\\s*([^\\s]+)\\s+([^,)\\s=]+)(?:\\s*=\\s*([^\",]+|(?:\"(?:(?=(?:\\\\?)).)+?\")))?,"s));
		const std::tstring paramList = funcSigBreakdown[3].str() + _T(',');
		size_t j = 1;
		for (auto i = std::regex_iterator<std::tstring::const_iterator>(
			paramList.cbegin(), paramList.cend(), paramListParser),
			rend = std::regex_iterator<std::tstring::const_iterator>();
			i != rend;
			++i, ++j)
		{
			const std::tstring paramTypeStr = (*i)[1].str();
			const std::tstring paramName = (*i)[2].str();
			const std::tstring paramNameL = ToLower(paramName);
			Type paramTypeValid;
			if (!StringToType(paramTypeValid, paramTypeStr.c_str()))
			{
				return CreateErrorT("%s: Parameter \"%s\" (index %zu) has unrecognised type \"%s\". Use Any, String, Integer, or Float.",
					_T(__FUNCTION__), paramName.c_str(), j, paramTypeStr.c_str());
			}

			const auto existingParam = std::find_if(params.cbegin(), params.cend(),
				[&](const Param& p) { return p.nameL == paramNameL; });
			if (existingParam != params.cend())
			{
				return CreateErrorT("%s: Parameter \"%s\" (index %zu) has the same name as previous parameter index %zu.",
					_T(__FUNCTION__), paramName.c_str(), j, (size_t)std::distance(params.cbegin(), existingParam));
			}

			// type funcCallBreakdown[i], name funcS
			params.push_back(Param(paramName.c_str(), paramTypeValid));
			if (!(*i)[3].matched)
			{
				// No default is fine as long as last param didn't have a default
				if (j > 1 && params[j - 2].defaultVal.type != Type::Any)
				{
					return CreateErrorT("%s: Parameter \"%s\" (index %zu) has no default value, but earlier parameter \"%s\" (index %zu) has a default."
						" All parameters with defaults must be at the end of the parameter list.",
						_T(__FUNCTION__), paramName.c_str(), j, params[j - 2].name.c_str(), j - 1);
				}
				continue;
			}

			// there was an =, but no content
			// Note that "= ," and "= )" will match, so we check for defaultVal being whitespace.
			// Whitespace is normally absorbed by the previous greedy matcher, but it will ignore one
			// whitespace if there is no default val, rather than failing the regex.
			// So, if it starts with whitespace, it ignored one to match, so boo.
			std::tstring defaultVal = (*i)[3].str();
			if (defaultVal.empty() || ::_istspace(defaultVal[0]))
			{
				return CreateErrorT("%s: Parameter \"%s\" (index %zu) has an empty default value; don't include the '=' if you want no default.",
					_T(__FUNCTION__), paramName.c_str(), j);
			}
			Param& justAdded = params.back();
			if (!Sub_ParseParamValue(_T(__FUNCTION__), defaultVal, justAdded, j, justAdded.defaultVal))
				return;
		}
	}

	// Too many parameters to ever call this with all of them - they should use scoped vars on start instead
	// Subtract 2: one for func name, one for repeat count, since last function will be a KRFunc
	if (params.size() > (size_t)::SDK->ExpressionInfos.back()->NumOfParams - 2)
		return CreateErrorT("%s: Too many parameters to run this function via expression. You have %zu parameters, but max is %hi. Consider using \"set scoped var on start\" action instead.",
			_T(__FUNCTION__), params.size(), ::SDK->ExpressionInfos.back()->NumOfParams - 2);

	std::shared_ptr<FunctionTemplate> func;
	const auto funcExisting = std::find_if(globals->functionTemplates.begin(), globals->functionTemplates.end(),
		[&](const auto& f) { return f->nameL == funcNameL; }
	);
	if (funcExisting == globals->functionTemplates.end())
	{
		func = std::make_shared<FunctionTemplate>(this, funcSigBreakdown[2].str().c_str(),(Expected)delayable, (Expected)repeatable, recursable != 0, returnTypeValid);
		globals->functionTemplates.push_back(func);
	}
	else
	{
		func = *funcExisting;
		func->delaying = (Expected)delayable;
		func->repeating = (Expected)repeatable;
		func->recursiveAllowed = recursable != 0;
		func->defaultReturnValue = Value(returnTypeValid);
	}
	func->params = params;
}
void Extension::Template_SetDefaultReturnN(const TCHAR * funcName)
{
	const std::shared_ptr<FunctionTemplate> funcExisting = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcName);
	if (!funcExisting)
		return;

	Value * const val = &funcExisting->defaultReturnValue;

	if (val->type == Type::String)
		free(val->data.string);

	val->data.string = NULL;
	val->dataSize = 0;
	val->type = Type::Any;
}
void Extension::Template_SetDefaultReturnI(const TCHAR * funcName, int value)
{
	const std::shared_ptr<FunctionTemplate> funcExisting = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcName);
	if (!funcExisting)
		return;

	Value * const val = &funcExisting->defaultReturnValue;

	if (val->type == Type::String)
		free(val->data.string);

	val->data.integer = value;
	val->dataSize = sizeof(int);
	val->type = Type::Integer;
}
void Extension::Template_SetDefaultReturnF(const TCHAR * funcName, float value)
{
	const std::shared_ptr<FunctionTemplate> funcExisting = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcName);
	if (!funcExisting)
		return;

	Value * const val = &funcExisting->defaultReturnValue;

	if (val->type == Type::String)
		free(val->data.string);

	val->data.decimal = value;
	val->dataSize = sizeof(float);
	val->type = Type::Float;
}
void Extension::Template_SetDefaultReturnS(const TCHAR * funcName, const TCHAR * newVal)
{
	const std::shared_ptr<FunctionTemplate> funcExisting = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcName);
	if (!funcExisting)
		return;

	Value * const val = &funcExisting->defaultReturnValue;

	TCHAR * const newDataCpy = _tcsdup(newVal);
	if (!newDataCpy)
		return CreateErrorT("Couldn't allocate memory.");

	if (val->type == Type::String)
		free(val->data.string);

	val->data.string = newDataCpy;
	val->dataSize = (_tcslen(newVal) + 1) * sizeof(TCHAR);
	val->type = Type::String;
}

void Extension::Template_Param_SetDefaultValueI(const TCHAR * funcName, const TCHAR * paramName, int paramValue, int useTheAnyType)
{
	const std::shared_ptr<FunctionTemplate> f = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcName);
	if (!f)
		return;

	Param * const p = Sub_GetTemplateParam(_T(__FUNCTION__), f, paramName);
	if (!p)
		return;
	Value * val = &p->defaultVal;
	if (val->type == Type::String)
		free(val->data.string);
	val->data.integer = paramValue;
	val->dataSize = sizeof(int);
	val->type = Type::Integer;
	p->type = useTheAnyType ? Type::Any : Type::Integer;
}
void Extension::Template_Param_SetDefaultValueF(const TCHAR * funcName, const TCHAR * paramName, float paramValue, int useTheAnyType)
{
	const std::shared_ptr<FunctionTemplate> f = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcName);
	if (!f)
		return;

	Param * const p = Sub_GetTemplateParam(_T(__FUNCTION__), f, paramName);
	if (!p)
		return;

	Value * const val = &p->defaultVal;
	if (val->type == Type::String)
		free(val->data.string);
	val->data.decimal = paramValue;
	val->dataSize = sizeof(float);
	val->type = Type::Float;
	p->type = useTheAnyType ? Type::Any : Type::Float;
}
void Extension::Template_Param_SetDefaultValueS(const TCHAR * funcName, const TCHAR * paramName, const TCHAR * paramValue, int useTheAnyType)
{
	const std::shared_ptr<FunctionTemplate> f = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcName);
	if (!f)
		return;

	Param * const p = Sub_GetTemplateParam(_T(__FUNCTION__), f, paramName);
	if (!p)
		return;

	TCHAR * newDataCpy = _tcsdup(paramValue);
	if (!newDataCpy)
		return CreateErrorT("Couldn't allocate memory.");

	Value * const val = &p->defaultVal;
	if (val->type == Type::String)
		free(val->data.string);
	val->data.string = newDataCpy;
	val->dataSize = (_tcslen(paramValue) + 1) * sizeof(TCHAR);
	val->type = Type::String;
	p->type = useTheAnyType ? Type::Any : Type::String;
}
void Extension::Template_Param_SetDefaultValueN(const TCHAR * funcName, const TCHAR * paramName, int useTheAnyType)
{
	const std::shared_ptr<FunctionTemplate> f = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcName);
	if (!f)
		return;

	Param * const p = Sub_GetTemplateParam(_T(__FUNCTION__), f, paramName);
	if (!p)
		return;

	Value * const val = &p->defaultVal;
	if (val->type == Type::String)
		free(val->data.string);
	val->data.integer = 0;
	val->dataSize = 0;
	val->type = Type::Any;

	// Leave as whatever type it was otherwise
	if (useTheAnyType)
		p->type = Type::Any;
}
void Extension::Template_SetScopedVarOnStartI(const TCHAR* funcName, const TCHAR* varName, int paramValue, int overrideWhenRecursing)
{
	if ((overrideWhenRecursing & 1) != overrideWhenRecursing)
		return CreateErrorT("Couldn't set scoped var on start; \"override when recursing\" must be 0 or 1, you supplied %i.", overrideWhenRecursing);

	ScopedVar* const p = Sub_GetOrCreateTemplateScopedVar(_T(__FUNCTION__), funcName, varName);
	if (!p)
		return;

	p->recursiveOverride = overrideWhenRecursing != 0;
	Value* const val = &p->defaultVal;
	if (val->type == Type::String)
		free(val->data.string);
	val->data.integer = paramValue;
	val->dataSize = sizeof(int);
	val->type = Type::Integer;
}
void Extension::Template_SetScopedVarOnStartF(const TCHAR* funcName, const TCHAR* varName, float paramValue, int overrideWhenRecursing)
{
	if ((overrideWhenRecursing & 1) != overrideWhenRecursing)
		return CreateErrorT("Couldn't set scoped var on start; \"override when recursing\" must be 0 or 1, you supplied %i.", overrideWhenRecursing);

	ScopedVar* const p = Sub_GetOrCreateTemplateScopedVar(_T(__FUNCTION__), funcName, varName);
	if (!p)
		return;

	p->recursiveOverride = overrideWhenRecursing != 0;
	Value* const val = &p->defaultVal;
	if (val->type == Type::String)
		free(val->data.string);
	val->data.decimal = paramValue;
	val->dataSize = sizeof(float);
	val->type = Type::Float;
}
void Extension::Template_SetScopedVarOnStartS(const TCHAR* funcName, const TCHAR* varName, const TCHAR* varValue, int overrideWhenRecursing)
{
	if ((overrideWhenRecursing & 1) != overrideWhenRecursing)
		return CreateErrorT("Couldn't set scoped var on start; \"override when recursing\" must be 0 or 1, you supplied %i.", overrideWhenRecursing);

	ScopedVar * const p = Sub_GetOrCreateTemplateScopedVar(_T(__FUNCTION__), funcName, varName);
	if (!p)
		return;

	TCHAR* const newDataCpy = _tcsdup(varValue);
	if (!newDataCpy)
		return CreateErrorT("Couldn't allocate memory.");

	p->recursiveOverride = overrideWhenRecursing != 0;
	Value* const val = &p->defaultVal;
	if (val->type == Type::String)
		free(val->data.string);
	val->data.string = newDataCpy;
	val->dataSize = (_tcslen(varValue) + 1) * sizeof(TCHAR);
	val->type = Type::String;
}
void Extension::Template_CancelScopedVarOnStart(const TCHAR* funcName, const TCHAR* varName)
{
	auto funcTemplate = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcName);
	if (!funcTemplate)
		return;
	if (varName[0] == _T('\0'))
		return CreateErrorT("%s: scoped var name is blank.", _T(__FUNCTION__));

	const std::tstring varNameL(ToLower(varName));
	const auto scopedVarIt = std::find_if(funcTemplate->scopedVarOnStart.begin(), funcTemplate->scopedVarOnStart.end(),
		[&](const ScopedVar& s) { return s.nameL == varNameL; });

	// already removed
	if (scopedVarIt == funcTemplate->scopedVarOnStart.end())
		return;
	funcTemplate->scopedVarOnStart.erase(scopedVarIt);
}
void Extension::Template_SetGlobalID(const TCHAR* funcName, const TCHAR* globalIDToRunOn)
{
	const std::shared_ptr<FunctionTemplate> f = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcName);
	if (!f)
		return;

	std::tstring key = _T("DarkScript"s) + globalIDToRunOn;
	const GlobalData * const gd = (GlobalData *)Runtime.ReadGlobal(key.c_str());
	if (gd == NULL)
		return CreateErrorT("Couldn't set global ID to \"%s\" for function template \"%s\", no matching extension with that global ID found.", globalIDToRunOn, funcName);
	if (gd->exts.size() == 0)
		return CreateErrorT("Couldn't set global ID to \"%s\" for function template \"%s\", global ID found but no extension using it currently.", globalIDToRunOn, funcName);
	if (gd->exts.size() > 1)
		CreateErrorT("Warning: global ID \"%s\" has more than one extension owning it. The first created one will be used.", globalIDToRunOn);
	f->globalID = globalIDToRunOn;
	f->ext = gd->exts[0];
}
void Extension::Template_SetEnabled(const TCHAR* funcName, int funcEnabled)
{
	if ((funcEnabled & 1) != funcEnabled)
		return CreateErrorT("Couldn't set function \"%s\" enabled; parameter should be 0 or 1, and was %d.", funcName, funcEnabled);

	const std::shared_ptr<FunctionTemplate> f = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcName);
	if (!f)
		return;
	f->isEnabled = funcEnabled != 0;
}
void Extension::Template_RedirectFunction(const TCHAR* funcName, const TCHAR* redirectFuncName)
{
	// Both blank, or both explicit func names that match
	if (!_tcsicmp(funcName, redirectFuncName))
		return CreateErrorT("Couldn't set function \"%s\" to redirect to \"%s\"; same function.", funcName, redirectFuncName);

	const std::shared_ptr<FunctionTemplate> f = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcName),
		f2 = Sub_GetFuncTemplateByName(_T(__FUNCTION__), redirectFuncName);
	if (!f || !f2)
		return;

	// Pulled running func with blank, and it matches the explicit func name
	if (f->nameL == f2->nameL)
		return CreateErrorT("Couldn't set function \"%s\" to redirect to \"%s\"; same function.", funcName, redirectFuncName);

	f->redirectFunc = f2->name;
	f->redirectFuncPtr = f2;
}
void Extension::Template_Loop(const TCHAR* loopName)
{
	if (loopName == _T('\0'))
		return CreateErrorT("%s: Empty loop name not allowed.");
	if (curLoopName[0] != _T('\0'))
		return CreateErrorT("%s: Can't run two internal loops at once; already running loop \"%s\", can't run loop \"%s\".", curLoopName.c_str(), loopName);

	internalLoopIndex = 0;
	for (const auto& f : globals->functionTemplates)
	{
		curFuncTemplateLoop = f;
		Runtime.GenerateEvent(7);
		++internalLoopIndex;
	}

	curFuncTemplateLoop = nullptr;
	internalLoopIndex = -1;
}

void Extension::DelayedFunctions_CancelByPrefix(const TCHAR * funcName)
{
	if (globals->pendingFuncs.empty())
		return;
	if (funcName[0] == _T('\0'))
		return globals->pendingFuncs.clear();

	const std::tstring funcNameL(ToLower(funcName));
	globals->pendingFuncs.erase(std::remove_if(globals->pendingFuncs.begin(), globals->pendingFuncs.end(),
		[&](const auto & f) { return !_tcsncmp(f->funcToRun->funcTemplate->nameL.c_str(), funcNameL.c_str(), funcNameL.size()); }), globals->pendingFuncs.end());
}
void Extension::DelayedFunctions_Loop(const TCHAR* loopName)
{
	if (loopName == _T('\0'))
		return CreateErrorT("%s: Empty loop name not allowed.");
	if (curLoopName[0] != _T('\0'))
		return CreateErrorT("%s: Can't run two internal loops at once; already running loop \"%s\", can't run loop \"%s\".", curLoopName.c_str(), loopName);

	internalLoopIndex = 0;
	for (const auto &f : globals->pendingFuncs)
	{
		curDelayedFuncLoop = f;
		Runtime.GenerateEvent(8);
		++internalLoopIndex;
	}

	curDelayedFuncLoop = nullptr;
	internalLoopIndex = -1;
}

/*
int numSelected(HeaderObject * obj);
int numSelectedQualifier(Extension* ext, int qualID);
extern std::tstringstream str;
extern int fixedValue;
extern bool inTestFunc;*/
void Extension::RunFunction_ActionDummy_Num(int result)
{
	// Ignore params, this action just allows user to run an expression's functions.

	/*RunObject* runObj = Runtime.RunObjPtrFromFixed(fixedValue ? fixedValue : result);

	str << _T("Event #"sv) << DarkEdif::GetCurrentFusionEventNum(this) <<
		_T(" inside action func: "sv) << numSelected(&runObj->roHo) <<
		_T(", rh2 event count = "sv) << rhPtr->rh2.EventCount << _T(", rh4 eventCount = "sv) << rhPtr->rh4.eventCount
		<< _T(", rh4 eventCountOR = "sv) << rhPtr->rh4.EventCountOR << _T(".\n"sv);
	if (!inTestFunc) {
		LOGI(_T("%s"), str.str().c_str());
		str.str(std::tstring());
	}
	*/
}
void Extension::RunFunction_ActionDummy_String(const TCHAR * result)
{
	// Ignore params, this action just allows user to run an expression's functions.

	/*
	if (Oi & 0x8000)
	{
		Oi &= 0x7FFF;	//Mask out the qualifier part
		int numberSelected = 0;

		qualToOi* CurrentQualToOiStart = (qualToOi*)rhPtr->QualToOiList;
		qualToOi* CurrentQualToOi = CurrentQualToOiStart;

		while (CurrentQualToOi->OiList >= 0)
		{
			objInfoList* CurrentOi = GetOILFromOI(CurrentQualToOi->OiList);
			numberSelected += CurrentOi->NumOfSelected;
			CurrentQualToOi = (qualToOi*)((char*)CurrentQualToOi + 4);
		}
		return numberSelected;
	}

	LOGI("======== starting loop.\n");
	struct dep
	{
		int qualID;
		int objCount;
		int instanceCount;
	};
	std::vector<dep> qualifiers;
	for (int i = 0; i < rhPtr->NumberOi; ++i)
	{
		objInfoList* oil = (objInfoList*)(((char*)rhPtr->OiList) + Runtime.ObjectSelection.oiListItemSize * i);
		for (size_t unwindToRunningFuncIndex = 0; unwindToRunningFuncIndex < std::size(oil->Qualifiers); unwindToRunningFuncIndex++) {
			if (oil->Qualifiers[unwindToRunningFuncIndex] == -1)
				break;

			auto dp = std::find_if(qualifiers.begin(), qualifiers.end(),
				[&](const dep& d) { return d.qualID == oil->Qualifiers[unwindToRunningFuncIndex]; });
			if (dp == qualifiers.cend())
			{
				qualifiers.push_back({ oil->Qualifiers[unwindToRunningFuncIndex], 1, 0 });
				dp = --qualifiers.end();
			}
			else
				++dp->objCount;
			LOGI("Object %s has qualifier ID %i.\n", oil->name, oil->Qualifiers[unwindToRunningFuncIndex]);

			short num = oil->Object;
			if (num != -1)
				dp->instanceCount += numSelected((rdPtr->rHo.AdRunHeader->ObjectList + num)->oblOffset);
		}
	}
	for (size_t i = 0; i < qualifiers.size(); i++)
		LOGI("Detected qualifier ID: %i, with %i objects (%i instances) using it.\n", qualifiers[i].qualID, qualifiers[i].objCount, qualifiers[i].instanceCount);

	/*
	int numberSelected = 0;

	for (size_t i = 0; i < qualifiers.size(); i++)
	{
		numberSelected = 0;
		qualToOi* CurrentQualToOiStart = (qualToOi*)&rhPtr->QualToOiList[i];
		qualToOi* CurrentQualToOi = CurrentQualToOiStart;

		while (CurrentQualToOi->OiList >= 0)
		{
			objInfoList* CurrentOi = Runtime.ObjectSelection.GetOILFromOI(CurrentQualToOi->OiList);
			numberSelected += numSelected((rdPtr->rHo.AdRunHeader->ObjectList + CurrentQualToOi->OiList)->oblOffset);
			CurrentQualToOi = (qualToOi*)((char*)CurrentQualToOi + 4);
		}
		LOGI("New calculation: qualifer ID %zu had %i instances.\n", i, numberSelected);
	}
	LOGI("======== ended loop.\n");
	qualToOi* q = (qualToOi*)rhPtr->QualToOiList;
	int i = 0;
	while (q != NULL)
	{
		LOGI(_T("Got q [%i] = %p.\n"), i++, q);
		q = &q[1];
	}
	qualToOi* CurrentQualToOi = CurrentQualToOiStart;

	Runtime.ObjectSelection.SelectOneObject
	RunObject* runObj = Runtime.RunObjPtrFromFixed(fixedValue);

	str << _T("Event #"sv) << DarkEdif::GetCurrentFusionEventNum(this) <<
		_T(" inside action func [qualifier]: "sv) << numSelectedQualifier(this, runObj->roHo.OiList->Qualifiers[0]) <<
		_T(", rh2 event count = "sv) << rhPtr->rh2.EventCount << _T(", rh4 eventCount = "sv) << rhPtr->rh4.eventCount
		<< _T(", rh4 eventCountOR = "sv) << rhPtr->rh4.EventCountOR << _T(".\n"sv);
	if (!inTestFunc) {
		LOGI(_T("%s"), str.str().c_str());
		str.str(std::tstring());
	}*/
}
void Extension::RunFunction_Foreach_Num(HeaderObject* obj, int dummy)
{
	// No expression executed when loading the "int dummy" used for this action, we don't know what func to run foreach on
	if (foreachFuncToRun == nullptr)
		return CreateErrorT("%s: Did not find any expression-function that was run in the foreach action.", _T(__FUNCTION__));

	// We store temp copies, so a Foreach expression's can run a Foreach itself without corrupting
	const auto funcToRun = foreachFuncToRun;
	foreachFuncToRun = nullptr;
	const short oil = (short)rdPtr->rHo.CurrentParam->evp.W[0];

	globals->runningFuncs.push_back(funcToRun);
	Sub_RunPendingForeachFunc(oil, funcToRun);
	globals->runningFuncs.erase(--globals->runningFuncs.cend());

	// The HeaderObject * points to the first instance of the selected objects this action
	// has in its parameters.
	// If you want your action to be called repeatedly, once for each object instance,
	// you just run as if the HeaderObject * is the only instance it's being run on, and the Fusion runtime
	// will cycle through each HeaderObject * that is the object instances passed by the event.
	// Otherwise, you disable ACTFLAGS_REPEAT, and loop it yourself.
	// Since we want the user to be able to cancel foreach loops midway, we'll do the loop ourselves.
	rhPtr->rh4.ActionStart->evtFlags &= ~ACTFLAGS_REPEAT;
}
void Extension::RunFunction_Foreach_String(HeaderObject* obj, const TCHAR* dummy)
{
	// No expression executed when loading the "int dummy" used for this action, we don't know what func to run foreach on
	if (foreachFuncToRun == nullptr)
		return CreateErrorT("%s: Did not find any expression-function that was run in the foreach action.", _T(__FUNCTION__));

	// We store temp copies, so a Foreach expression's can run a Foreach itself without corrupting
	auto funcToRun = foreachFuncToRun;
	foreachFuncToRun = nullptr;
	short oil = (short)rdPtr->rHo.CurrentParam->evp.W[0];

	globals->runningFuncs.push_back(funcToRun);
	Sub_RunPendingForeachFunc(oil, funcToRun);
	globals->runningFuncs.erase(--globals->runningFuncs.cend());

	// For an explanation of this flag, see RunFunction_Foreach_Num()
	rhPtr->rh4.ActionStart->evtFlags &= ~ACTFLAGS_REPEAT;
}
void Extension::RunFunction_Delayed_Num_MS(int timeFirst, int numRepeats, int timeSubsequent, int crossFrames, int funcDummy)
{
	// This action does nothing. Actual handling of this parameters happens when funcDummy is being evaluated, and VariableFunction() is called.
	// The delayed function made by this action is handled under Extension::Handle().
}
void Extension::RunFunction_Delayed_String_MS(int timeFirst, int numRepeats, int timeSubsequent, int crossFrames, const TCHAR* funcDummy)
{
	// This action does nothing. Actual handling of this parameters happens when funcDummy is being evaluated, and VariableFunction() is called.
	// The delayed function made by this action is handled under Extension::Handle().
}
void Extension::RunFunction_Delayed_Num_Ticks(int timeFirst, int numRepeats, int timeSubsequent, int crossFrames, int funcDummy)
{
	// This action does nothing. Actual handling of this parameters happens when funcDummy is being evaluated, and VariableFunction() is called.
	// The delayed function made by this action is handled under Extension::Handle().
}
void Extension::RunFunction_Delayed_String_Ticks(int timeFirst, int numRepeats, int timeSubsequent, int crossFrames, const TCHAR* funcDummy)
{
	// This action does nothing. Actual handling of this parameters happens when funcDummy is being evaluated, and VariableFunction() is called.
	// The delayed function made by this action is handled under Extension::Handle().
}
void Extension::RunFunction_Script(const TCHAR* script)
{
	// TODO: Create If, Else etc sort of functions, + operators, nested funcs, etc
	std::basic_regex<TCHAR> funcCallMatcher(_T("([^\\s(]+)\\s*\\((?:\\s*([^\\s]+)\\s+([^,\\s]*),\\s*)*(?:\\s*([^\\s]+)\\s+([^,)\\s]*))?\\)"s));

	std::match_results<std::tstring::iterator> funcCallBreakdown;
	std::tstring test(script);
	if (!std::regex_match(test.begin(), test.end(), funcCallBreakdown, funcCallMatcher))
		return CreateErrorT("%s: Function script \"%s\" not parseable.", _T(__FUNCTION__), script);

	std::tstring funcName = funcCallBreakdown[1].str();
	std::tstring funcNameL(ToLower(funcName));
	// Make sure it's not a KRFuncXXX() function name. That would work, but the script engine
	// (used in "run text as script") will find it ambiguous.
	const std::basic_regex<TCHAR> isKRFunc(_T("(k?r?f?)func([fis]*)(\\$?)"s));
	std::match_results<std::tstring::const_iterator> isKRFuncCallBreakdown;
	if (std::regex_match(funcNameL.cbegin(), funcNameL.cend(), isKRFuncCallBreakdown, isKRFunc))
	{
		const std::tstring prefix = isKRFuncCallBreakdown[1].str();
		const std::tstring params = isKRFuncCallBreakdown[2].str();
		const std::tstring suffix = isKRFuncCallBreakdown[3].str();

		return CreateErrorT("%s: Function name \"%s\" is invalid; KRFuncXX format will confuse the script parser.", _T(__FUNCTION__), funcName.c_str());
	}

	std::shared_ptr<FunctionTemplate> funcTemplate;
	// Match by name
	auto res = std::find_if(globals->functionTemplates.begin(), globals->functionTemplates.end(),
		[&](const std::shared_ptr<FunctionTemplate>& f) { return f->nameL == funcNameL;
	});
	
	if (res == globals->functionTemplates.end())
	{
		if (funcsMustHaveTemplate)
			return CreateErrorT("%s: Function script uses function name \"%s\", which has no template.", _T(__FUNCTION__), funcName.c_str());

		funcTemplate = std::make_shared<FunctionTemplate>(this, funcName.c_str(), Expected::Either, Expected::Either, false, Type::Any);
		TCHAR name[3] = { _T('a'), _T('\0') };
		// skip index 0, full match, and index 1, func name
		for (size_t i = 2; i < (size_t)funcCallBreakdown.size(); i++, ++name[0])
		{
			// Note the ++name[0] in for(;;><), gives variable names a, b, c
			funcTemplate->params.push_back(Param(name, Type::Any));
		}
		lastReturn = Value(Type::Any);
	}
	else
	{
		funcTemplate = *res;
		if (funcTemplate->delaying == Expected::Always)
			return CreateErrorT("%s: Function script uses function name \"%s\", which is expected to be called delayed only.", _T(__FUNCTION__), funcName.c_str());
		if (funcTemplate->repeating == Expected::Always)
			return CreateErrorT("%s: Function script uses function name \"%s\", which is expected to be called repeating only.", _T(__FUNCTION__), funcName.c_str());

		if (!funcTemplate->isEnabled)
		{
			lastReturn = funcTemplate->defaultReturnValue;
			if (funcTemplate->defaultReturnValue.type == Type::Any)
				CreateErrorT("%s: Function script uses function name \"%s\", which is set to disabled, and has no default return value.", _T(__FUNCTION__), funcName.c_str());
			return;
		}
	}

	std::tstring info = _T("Script run of ") + funcName;
	std::vector<Value> values;
	Value writeTo(Type::Any);
	for (size_t i = 0; i < funcCallBreakdown.size() - 2U; i++)
	{
		if (!Sub_ParseParamValue(info.c_str(), funcCallBreakdown[i + 2].str(),
			funcTemplate->params[i], i, writeTo))
		{
			return;
		}
		values.push_back(writeTo);
	}

	const std::shared_ptr<RunningFunction> runningFunc = std::make_shared<RunningFunction>(funcTemplate, true, 0);
	for (size_t i = 0; i < values.size(); i++)
		runningFunc->paramValues[i] = values[i];

	std::vector<FusionSelectedObjectListCache> selObjList;
	evt_SaveSelectedObjects(selObjList);
	runningFunc->runLocation = Sub_GetLocation(27);
	assert(::SDK->ActionFunctions[27] == Edif::MemberFunctionPointer(&Extension::RunFunction_Script));
	ExecuteFunction(nullptr, runningFunc);
	evt_RestoreSelectedObjects(selObjList, true);
}

void Extension::RunningFunc_Params_Loop(const TCHAR* loopName, int includeNonPassed)
{
	if (loopName == _T('\0'))
		return CreateErrorT("%s: Empty loop name not allowed.");
	if (curLoopName[0] != _T('\0'))
		return CreateErrorT("%s: Can't run two internal loops at once; already running loop \"%s\", can't run loop \"%s\".", curLoopName.c_str(), loopName);
	if ((includeNonPassed & 1) != includeNonPassed)
		return CreateErrorT("%s: Can't run params loop; \"include non-passed\" parameter was %d, not 0 or 1.", includeNonPassed);

	const auto rf = Sub_GetRunningFunc(_T(__FUNCTION__), _T(""));
	if (!rf)
		return;

	size_t i = 0;
	for (auto& p : rf->funcTemplate->params)
	{
		if (includeNonPassed == 0 && i++ > rf->numPassedParams)
			break;
		curParamLoop = &p;
		Runtime.GenerateEvent(9);
		++internalLoopIndex;
	}
	curParamLoop = nullptr;
	internalLoopIndex = -1;
}
void Extension::RunningFunc_ScopedVar_Loop(const TCHAR* loopName, int includeInherited)
{
	if (loopName == _T('\0'))
		return CreateErrorT("%s: Empty loop name not allowed.");
	if (curLoopName[0] != _T('\0'))
		return CreateErrorT("%s: Can't run two internal loops at once; already running loop \"%s\", can't run loop \"%s\".", curLoopName.c_str(), loopName);
	if ((includeInherited & 1) != includeInherited)
		return CreateErrorT("%s: Can't run params loop; \"include inherited\" parameter was %d, not 0 or 1.", includeInherited);

	const auto rf = Sub_GetRunningFunc(_T(__FUNCTION__), _T(""));
	if (!rf)
		return;

	internalLoopIndex = 0;
	for (const auto& s : globals->scopedVars)
	{
		if (includeInherited == 0 && s.level != globals->runningFuncs.size())
			continue;
		curScopedVarLoop = &s;
		Runtime.GenerateEvent(10);
		++internalLoopIndex;
	}
	curScopedVarLoop = nullptr;
	internalLoopIndex = -1;
}
void Extension::RunningFunc_SetReturnI(int value)
{
	const auto rf = Sub_GetRunningFunc(_T(__FUNCTION__), _T(""));
	if (!rf)
		return;

	Value * const val = &rf->returnValue;
	// TODO: Complain or ignore delayed func return types?
	// Delayed func return types will be Any.
	if (val->type != Type::Integer)
	{
		std::tstring typeName = _T("no return value"s);
		if (val->type != Type::Any)
			typeName = TypeToString(val->type) + _T(" return type"s);
		return CreateErrorT("Can't return type %s from function %s, expected %s.",
			_T("integer"), rf->funcTemplate->name.c_str(), typeName.c_str());
	}
	if (val->type == Type::String)
		free(val->data.string);

	val->data.integer = value;
	val->dataSize = sizeof(int);
	val->type = Type::Integer;
}
void Extension::RunningFunc_SetReturnF(float value)
{
	const auto rf = Sub_GetRunningFunc(_T(__FUNCTION__), _T(""));
	if (!rf)
		return;

	Value * const val = &rf->returnValue;
	if (val->type != Type::Float)
	{
		std::tstring typeName = _T("no return value");
		if (val->type != Type::Any)
			typeName = TypeToString(val->type) + std::tstring(_T(" return type"));
		return CreateErrorT("Can't return type %s from function %s, expected %s.",
			_T("float"), rf->funcTemplate->name.c_str(), typeName.c_str());
	}
	if (val->type == Type::String)
		free(val->data.string);

	val->data.decimal = value;
	val->dataSize = sizeof(float);
	val->type = Type::Float;
}
void Extension::RunningFunc_SetReturnS(const TCHAR * newVal)
{
	const auto rf = Sub_GetRunningFunc(_T(__FUNCTION__), _T(""));
	if (!rf)
		return;

	Value * const val = &rf->returnValue;
	if (val->type != Type::String)
	{
		std::tstring typeName = _T("no return value");
		if (val->type != Type::Any)
			typeName = TypeToString(val->type) + std::tstring(_T(" return type"));
		return CreateErrorT("Can't return type %s from function %s, expected %s.",
			_T("string"), rf->funcTemplate->name.c_str(), typeName.c_str());
	}

	// TODO: realloc, combine size_t measurements for dup alloc and copy

	TCHAR * const newDataCpy = _tcsdup(newVal);
	if (!newDataCpy)
		return CreateErrorT("Couldn't allocate memory.");

	if (val->type == Type::String)
		free(val->data.string);

	val->data.string = newDataCpy;
	val->dataSize = (_tcslen(newVal) + 1) * sizeof(TCHAR);
	val->type = Type::String;
}
void Extension::RunningFunc_ScopedVar_SetI(const TCHAR* paramName, int newVal)
{
	const Param* param = nullptr;
	Value* val = Sub_CheckScopedVarAvail(_T(__FUNCTION__), paramName, Expected::Either, false, &param);
	if (!val)
	{
		globals->scopedVars.push_back(ScopedVar(paramName, Type::String, true, globals->runningFuncs.size()));
		param = &globals->scopedVars.back();
		val = &globals->scopedVars.back().defaultVal;
	}

	// Check the type is int, or convertible to int
	if (param->type != Type::Integer && param->type != Type::Any)
	{
		return CreateErrorT("%s: param/scoped var %s does not accept integer type.",
			_T(__FUNCTION__), param->name.c_str());
	}

	if (val->type == Type::String)
		free(val->data.string);
	val->data.integer = newVal;
	val->dataSize = sizeof(newVal);
	val->type = Type::Integer;
}
void Extension::RunningFunc_ScopedVar_SetF(const TCHAR* paramName, float newVal)
{
	const Param* param = nullptr;
	Value* val = Sub_CheckScopedVarAvail(_T(__FUNCTION__), paramName, Expected::Either, false, &param);
	if (!val)
	{
		globals->scopedVars.push_back(ScopedVar(paramName, Type::Float, true, globals->runningFuncs.size()));
		param = &globals->scopedVars.back();
		val = &globals->scopedVars.back().defaultVal;
	}

	// Check the type is float, or convertible to float
	if (param->type != Type::Float && param->type != Type::Any)
	{
		return CreateErrorT("%s: param/scoped var %s does not accept float type.",
			_T(__FUNCTION__), param->name.c_str());
	}

	if (val->type == Type::String)
		free(val->data.string);
	val->data.decimal = newVal;
	val->dataSize = sizeof(newVal);
	val->type = Type::Float;
}
void Extension::RunningFunc_ScopedVar_SetS(const TCHAR* paramName, const TCHAR* newVal)
{
	const Param* param = nullptr;
	Value* val = Sub_CheckScopedVarAvail(_T(__FUNCTION__), paramName, Expected::Either, false, &param);
	if (!val)
	{
		globals->scopedVars.push_back(ScopedVar(paramName, Type::String, true, globals->runningFuncs.size()));
		param = &globals->scopedVars.back();
		val = &globals->scopedVars.back().defaultVal;
	}

	// Check the type is string, or convertible to string
	if (param->type != Type::String && param->type != Type::Any)
	{
		return CreateErrorT("%s: param/scoped var %s does not accept string type.",
			_T(__FUNCTION__), param->name.c_str());
	}

	// TODO: realloc, combine size_t measurements for dup alloc and copy

	TCHAR* const newDataCpy = _tcsdup(newVal);
	if (!newDataCpy)
		return CreateErrorT("Couldn't allocate memory.");

	if (val->type == Type::String)
		free(val->data.string);
	val->data.string = newDataCpy;
	val->dataSize = (_tcslen(newVal) + 1) * sizeof(TCHAR);
	val->type = Type::String;
}
void Extension::RunningFunc_StopFunction(int cancelCurrentIteration, int cancelNextIterations, int cancelForeach)
{
	if ((cancelCurrentIteration & 1) != cancelCurrentIteration)
		return CreateErrorT("Use of StopFunction with an incorrect \"cancel current iteration\" parameter %i. Must be 0 or 1.", cancelCurrentIteration);
	if ((cancelNextIterations & 1) != cancelNextIterations)
		return CreateErrorT("Use of StopFunction with an incorrect \"cancel next iterations\" parameter %i. Must be 0 or 1.", cancelNextIterations);
	if ((cancelForeach & 1) != cancelForeach)
		return CreateErrorT("Use of StopFunction with an incorrect \"cancel foreach loop\" parameter %i. Must be 0 or 1.", cancelForeach);

	const bool cancelCurrentB = cancelCurrentIteration != 0;
	const bool cancelNextB = cancelNextIterations != 0;
	const bool cancelForeachB = cancelForeach != 0;
	if (!cancelCurrentB && !cancelNextB && !cancelForeachB)
		return CreateErrorT("Use of StopFunction with all cancel parameters as 0. This will have no effect.");

	const auto rf = Sub_GetRunningFunc(_T(__FUNCTION__), _T(""));
	if (!rf)
		return;

	if (cancelCurrentB)
		rf->currentIterationTriggering = false; // stops instantly in On Function event; next On Function check returns false
	if (cancelNextB)
		rf->nextRepeatIterationTriggering = false; // stops after all On Function conds for this event loop finishes
	if (cancelForeachB)
		rf->foreachTriggering = false;
}
void Extension::RunningFunc_ChangeRepeatSetting(int newRepeatIndex, int newRepeatCount, int ignoreExistingCancel)
{
	if ((ignoreExistingCancel & 1) != ignoreExistingCancel)
		return CreateErrorT("Use of ChangeRepeatSetting with an incorrect \"ignore existing cancel\" parameter %i. Must be 0 or 1.", ignoreExistingCancel);

	if (newRepeatIndex < 0)
		return CreateErrorT("Use of ChangeRepeatSetting with a new repeat index of %i; must be more or equal to 0.", newRepeatIndex);
	if (newRepeatCount < 1)
		return CreateErrorT("Use of ChangeRepeatSetting with a new repeat count of %i; must be more or equal to 1.", newRepeatCount);
	if (newRepeatIndex >= newRepeatCount)
		return CreateErrorT("Use of ChangeRepeatSetting with a new repeat index of %i, higher than new repeat count %i.", newRepeatIndex, newRepeatCount);

	const auto rf = Sub_GetRunningFunc(_T(__FUNCTION__), _T(""));
	if (!rf || !rf->active)
		return;

	rf->index = newRepeatIndex;
	rf->numRepeats = newRepeatCount;
	if (!rf->nextRepeatIterationTriggering && ignoreExistingCancel == 0)
		rf->nextRepeatIterationTriggering = true;
}
void Extension::RunningFunc_Abort(const TCHAR* error, const TCHAR* funcToUnwindTo)
{
	if (error[0] == _T('\0'))
		return CreateErrorT("Can't abort with an empty reason.");

	const auto rf = Sub_GetRunningFunc(_T(__FUNCTION__), _T(""));
	if (!rf)
		return;
	if (!rf->active)
		return CreateErrorT("Can't abort twice. Second reason discarded.");

	size_t unwindToRunningFuncIndex = 0;
	if (funcToUnwindTo[0] != _T('\0'))
	{
		const std::tstring funcToUnwindToL = ToLower(funcToUnwindTo);
		const auto q = std::find_if(globals->runningFuncs.crbegin(), globals->runningFuncs.crend(), [&](const auto& f)
			{ return f->funcTemplate->nameL == funcToUnwindToL; });
		if (q == globals->runningFuncs.crend())
			return CreateErrorT("Aborting function \"%s\" to function \"%s\" failed; unwind function \"%s\" not in call stack.", rf->funcTemplate->name.c_str(), funcToUnwindTo, funcToUnwindTo);
		unwindToRunningFuncIndex = globals->runningFuncs.size() - std::distance(globals->runningFuncs.crbegin(), q);
		assert((long)unwindToRunningFuncIndex >= 0);
	}

	for (size_t i = globals->runningFuncs.size() - 1; i >= unwindToRunningFuncIndex; --i)
	{
		const auto& fa = globals->runningFuncs[i];
		fa->abortReason = error;
		fa->currentIterationTriggering = false;
		fa->nextRepeatIterationTriggering = false;
		fa->active = false;

		if (fa->returnValue.type == Type::Any)
		{
			fa->returnValue = fa->funcTemplate->defaultReturnValue;
			// No default return
			if (fa->returnValue.type == Type::Any)
				CreateErrorT("Warning: Aborted function \"%s\" from function \"%s\", Fusion event line %d, with no default return and no set return value.",
					fa->funcTemplate->name.c_str(), rf->funcTemplate->name.c_str(), DarkEdif::GetCurrentFusionEventNum(this));
		}
	}
}

void Extension::Logging_SetLevel(const TCHAR* funcNames, const TCHAR* logLevel)
{
	CreateErrorT("%s not implemented", _T(__FUNCTION__));
}
