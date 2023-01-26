
#include "Common.h"

const TCHAR * Extension::Logging_GetDarkScriptError()
{
	return Runtime.CopyString(curError.c_str());
}
const TCHAR* Extension::Logging_GetAbortReason()
{
	// TODO: Shouldn't this be the called func's abort reason, not this one?
	const auto rf = Sub_GetRunningFunc(_T(__FUNCTION__), _T(""));
	return Runtime.CopyString(rf ? rf->abortReason.c_str() : _T(""));
}
const TCHAR* Extension::Logging_GetCurrentLog(int clearLog)
{
	const std::tstring log = curLog.str();
	if (clearLog == 1)
		curLog.str(std::tstring());
	return Runtime.CopyString(log.c_str());
}

int Extension::RunningFunc_GetRepeatIndex(const TCHAR * funcNameOrBlank)
{
	const auto rf = Sub_GetRunningFunc(_T(__FUNCTION__), funcNameOrBlank);
	return rf ? rf->index : -1;
}
int Extension::RunningFunc_GetNumRepeatsLeft(const TCHAR* funcNameOrBlank)
{
	const auto rf = Sub_GetRunningFunc(_T(__FUNCTION__), funcNameOrBlank);
	return rf ? rf->numRepeats - rf->index : -1;
}
int Extension::RunningFunc_GetNumRepeatsTotal(const TCHAR* funcNameOrBlank)
{
	const auto rf = Sub_GetRunningFunc(_T(__FUNCTION__), funcNameOrBlank);
	return rf ? rf->numRepeats : -1;
}
int Extension::RunningFunc_ForeachFV(const TCHAR* funcNameOrBlank)
{
	const auto rf = Sub_GetRunningFunc(_T(__FUNCTION__), funcNameOrBlank);
	return rf ? rf->currentForeachObjFV : -1;
}
int Extension::RunningFunc_NumParamsPassed(const TCHAR* funcNameOrBlank)
{
	const auto rf = Sub_GetRunningFunc(_T(__FUNCTION__), funcNameOrBlank);
	return rf ? rf->numPassedParams : -1;
}
int Extension::RunningFunc_ScopedVar_GetI(const TCHAR* scopedVarName)
{
	const Value * const val = Sub_CheckScopedVarAvail(_T(__FUNCTION__), scopedVarName, Expected::Either, true);
	return !val ? 0 : Sub_GetValAsInteger(*val);
}
float Extension::RunningFunc_ScopedVar_GetF(const TCHAR* scopedVarName)
{
	const Value * const val = Sub_CheckScopedVarAvail(_T(__FUNCTION__), scopedVarName, Expected::Either, true);
	return !val ? 0.f : Sub_GetValAsFloat(*val);
}
const TCHAR* Extension::RunningFunc_ScopedVar_GetS(const TCHAR* scopedVarName)
{
	const Value * const val = Sub_CheckScopedVarAvail(_T(__FUNCTION__), scopedVarName, Expected::Either, true);
	return Runtime.CopyString(!val ? _T("") : Sub_GetValAsString(*val).c_str());
}
int Extension::RunningFunc_GetParamValueByIndexI(int paramIndex)
{
	const Value * const val = Sub_CheckParamAvail(_T(__FUNCTION__), paramIndex);
	return !val ? 0 : Sub_GetValAsInteger(*val);
}
float Extension::RunningFunc_GetParamValueByIndexF(int paramIndex)
{
	const Value * const val = Sub_CheckParamAvail(_T(__FUNCTION__), paramIndex);
	return !val ? 0.f : Sub_GetValAsFloat(*val);
}

const TCHAR* Extension::RunningFunc_GetParamValueByIndexS(int paramIndex)
{
	const Value * const val = Sub_CheckParamAvail(_T(__FUNCTION__), paramIndex);
	return Runtime.CopyString(!val ? _T("") : Sub_GetValAsString(*val).c_str());
}
const TCHAR * Extension::RunningFunc_GetAllParamsAsText(const TCHAR* funcNameOrBlank, const TCHAR * separatorPtr, int annotate)
{
	const auto rf = Sub_GetRunningFunc(_T(__FUNCTION__), funcNameOrBlank);
	if (!rf)
		return Runtime.CopyString(_T(""));

	size_t i = 0;
	std::tstringstream output;
	std::tstring_view separator(separatorPtr);
	for (const auto & p : rf->paramValues)
	{
		if (!annotate)
		{
			if (p.type == Type::Integer)
				output << p.data.integer << separator;
			else if (p.type == Type::Float)
				output << p.data.decimal << separator;
			else
				output << p.data.string << separator;
		}
		else
		{
			output << rf->funcTemplate->params[i].name << _T(" = "sv);
			if (p.type == Type::Integer)
				output << p.data.integer << separator;
			else if (p.type == Type::Float)
				output << p.data.decimal << _T('f') << separator;
			else
			{
				std::tstring escapeMe = p.data.string;
				Sub_ReplaceAllString(escapeMe, _T("\r"sv), _T(""sv));
#ifdef _WIN32
				Sub_ReplaceAllString(escapeMe, _T("\n"sv), _T("\\r\\n"sv));
#else
				Sub_ReplaceAllString(escapeMe, _T("\n"sv), _T("\\n"sv));
#endif
				Sub_ReplaceAllString(escapeMe, _T("\t"sv), _T("\\t"sv));
				Sub_ReplaceAllString(escapeMe, _T("\""sv), _T("\\\""sv));
				Sub_ReplaceAllString(escapeMe, _T("\\"sv), _T("\\\\"sv));

				output << _T('"') << escapeMe << _T('"') << separator;
			}
		}
	}
	std::tstring outputStr(output.str());
	if (!outputStr.empty())
		outputStr.resize(outputStr.size() - separator.size());

	return Runtime.CopyString(outputStr.c_str());
}
const TCHAR* Extension::RunningFunc_GetCallStack(int mostRecentAtBottom, int rewindCount)
{
	if (globals->runningFuncs.empty())
		return CreateErrorT("Couldn't get call stack; no functions running."), Runtime.CopyString(_T(""));
	if (rewindCount == 0 || rewindCount < -1)
		return CreateErrorT("Couldn't get call stack; rewind count %i is invalid.", rewindCount), Runtime.CopyString(_T(""));
	if ((mostRecentAtBottom & 1) != mostRecentAtBottom)
		return CreateErrorT("Couldn't get call stack; most recent at bottom param %i is invalid, must be 0 or 1.", mostRecentAtBottom), Runtime.CopyString(_T(""));

	if (rewindCount == -1)
		rewindCount = (int)globals->runningFuncs.size();

#if _WIN32
	const std::tstring_view nl = _T("\r\n"sv);
#else
	const std::tstring_view nl = _T("\n"sv);
#endif
	std::tstringstream str;
	size_t j = 0;
	if (mostRecentAtBottom == 0) {
		for (auto i = globals->runningFuncs.crbegin(); i != globals->runningFuncs.crend() && j < (size_t)rewindCount; ++i, ++j)
			str << (*i)->funcTemplate->name << _T(" called from "sv) << (*i)->runLocation << nl;
	} else {
		for (auto i = globals->runningFuncs.cbegin(); i != globals->runningFuncs.cend() && j < (size_t)rewindCount; ++i, ++j)
			str << (*i)->funcTemplate->name << _T(" called from "sv) << (*i)->runLocation << nl;
	}
	std::tstring str2 = str.str();
	str2.resize(str2.size() - nl.size());
	return Runtime.CopyString(str2.c_str());
}

int Extension::InternalLoop_GetIndex()
{
	if (curLoopName[0] == _T('\0'))
		return CreateErrorT("Couldn't read internal loop index: no internal loop running."), -1;
	return (int)internalLoopIndex;
}
const TCHAR* Extension::InternalLoop_GetVarName()
{
	if (curLoopName[0] == _T('\0'))
		return CreateErrorT("Couldn't read internal loop param/scoped var name: no internal loop running."), Runtime.CopyString(_T(""));
	if (curParamLoop == NULL && curScopedVarLoop == NULL)
		return CreateErrorT("Couldn't read internal loop param/scoped var name: internal loop \"%s\" is not a param/scoped var loop.", curLoopName.c_str()), Runtime.CopyString(_T(""));
	return Runtime.CopyString((curParamLoop ? curParamLoop->name : curScopedVarLoop->name).c_str());
}
const TCHAR* Extension::InternalLoop_GetVarType()
{
	if (curLoopName[0] == _T('\0'))
		return CreateErrorT("Couldn't read internal loop param/scoped var type: no internal loop running."), Runtime.CopyString(_T(""));
	if (curParamLoop == NULL && curScopedVarLoop == NULL)
		return CreateErrorT("Couldn't read internal loop param/scoped var type: internal loop \"%s\" is not a param/scoped var loop.", curLoopName.c_str()), Runtime.CopyString(_T(""));
	return Runtime.CopyString(TypeToString(curParamLoop ? curParamLoop->defaultVal.type : curScopedVarLoop->defaultVal.type));
}

const TCHAR* Extension::FuncTemplate_GetFunctionName()
{
	const auto ft = Sub_GetFuncTemplateByName(_T(__FUNCTION__), _T(""));
	return Runtime.CopyString(ft ? ft->name.c_str() : _T(""));
}
int Extension::FuncTemplate_GetNumRequiredParams(const TCHAR* funcNameOrBlank)
{
	const auto ft = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcNameOrBlank);
	if (!ft)
		return -1;
	// Type of Any is unset default value, so is required param
	return (int)std::count_if(ft->params.cbegin(), ft->params.cend(), [](const Param& p) {
		return p.defaultVal.type == Type::Any; });
}
int Extension::FuncTemplate_GetNumPossibleParams(const TCHAR * funcNameOrBlank)
{
	const auto ft = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcNameOrBlank);
	return ft ? (int)ft->params.size() : -1;
}
const TCHAR* Extension::FuncTemplate_GetReturnType(const TCHAR* funcNameOrBlank)
{
	const auto ft = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcNameOrBlank);
	return Runtime.CopyString(ft ? TypeToString(ft->defaultReturnValue.type) : _T(""));
}
int Extension::FuncTemplate_ShouldRepeat(const TCHAR * funcNameOrBlank)
{
	const auto f = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcNameOrBlank);
	return f ? (int)f->repeating : -1;
}
int Extension::FuncTemplate_ShouldBeDelayed(const TCHAR * funcNameOrBlank)
{
	const auto f = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcNameOrBlank);
	return f ? (int)f->delaying : -1;
}
int Extension::FuncTemplate_RecursionAllowed(const TCHAR * funcNameOrBlank)
{
	const auto f = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcNameOrBlank);
	return f ? (f->recursiveAllowed ? 1 : 0) : -1;
}
int Extension::FuncTemplate_IsEnabled(const TCHAR* funcNameOrBlank)
{
	const auto f = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcNameOrBlank);
	return f ? (f->isEnabled ? 1 : 0) : -1;
}

const TCHAR* Extension::FuncTemplate_GetRedirectFuncName(const TCHAR* funcNameOrBlank)
{
	const auto ft = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcNameOrBlank);
	return Runtime.CopyString(ft ? ft->redirectFunc.c_str() : _T(""));
}
const TCHAR* Extension::FuncTemplate_GlobalID(const TCHAR* funcNameOrBlank)
{
	const auto ft = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcNameOrBlank);
	return Runtime.CopyString(ft ? ft->globalID.c_str() : _T(""));
}

const TCHAR * Extension::FuncTemplate_ParamNameByIndex(const TCHAR * funcNameOrBlank, int paramIndex)
{
	const auto f = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcNameOrBlank);
	const Param * const p = Sub_GetTemplateParam(_T(__FUNCTION__), f, paramIndex);
	return Runtime.CopyString(p ? p->name.c_str() : _T(""));
}
const TCHAR * Extension::FuncTemplate_ParamTypeByIndex(const TCHAR * funcNameOrBlank, int paramIndex)
{
	const auto f = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcNameOrBlank);
	const Param * const p = Sub_GetTemplateParam(_T(__FUNCTION__), f, paramIndex);
	return Runtime.CopyString(p ? TypeToString(p->defaultVal.type) : _T(""));
}
const TCHAR * Extension::FuncTemplate_ParamDefaultValByIndex(const TCHAR * funcNameOrBlank, int paramIndex)
{
	const auto f = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcNameOrBlank);
	const Param * const p = Sub_GetTemplateParam(_T(__FUNCTION__), f, paramIndex);
	return Runtime.CopyString(p ? Sub_GetValAsString(p->defaultVal).c_str() : _T(""));
}
int Extension::FuncTemplate_ParamIndexByName(const TCHAR * funcNameOrBlank, const TCHAR * paramName)
{
	const auto f = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcNameOrBlank);
	const Param * const p = Sub_GetTemplateParam(_T(__FUNCTION__), f, paramName);
	if (!p)
		return -1;

	for (size_t i = 0; i < f->params.size(); i++)
		if (&f->params[i] == p)
			return i;

	CreateErrorT("Failure at line %i, file %s.", __LINE__, _T(__FILE__));
	return -1;
}
const TCHAR * Extension::FuncTemplate_ParamTypeByName(const TCHAR * funcNameOrBlank, const TCHAR * paramName)
{
	const std::shared_ptr<FunctionTemplate> f = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcNameOrBlank);
	const Param * const p = Sub_GetTemplateParam(_T(__FUNCTION__), f, paramName);
	return Runtime.CopyString(p ? TypeToString(p->defaultVal.type) : _T(""));
}
const TCHAR * Extension::FuncTemplate_ParamDefaultValByName(const TCHAR * funcNameOrBlank, const TCHAR * paramName)
{
	const std::shared_ptr<FunctionTemplate> f = Sub_GetFuncTemplateByName(_T(__FUNCTION__), funcNameOrBlank);
	const Param * p = Sub_GetTemplateParam(_T(__FUNCTION__), f, paramName);
	return Runtime.CopyString(p ? Sub_GetValAsString(p->defaultVal).c_str() : _T(""));
}
int Extension::LastReturn_AsInt()
{
	return Sub_GetValAsInteger(lastReturn);
}
float Extension::LastReturn_AsFloat()
{
	return Sub_GetValAsFloat(lastReturn);
}
const TCHAR* Extension::LastReturn_AsString()
{
	return Runtime.CopyString(Sub_GetValAsString(lastReturn).c_str());
}
const TCHAR* Extension::LastReturn_Type()
{
	if (lastReturn.dataSize == 0)
		return Runtime.CopyString(_T("void"));
	return Runtime.CopyString(TypeToString(lastReturn.type));
}

std::tstringstream str;
int fixedValue;
int origEventCount, origEventCountOR;
int numSelected(HeaderObject * obj)
{
	auto poil = obj->OiList;
	LOGI(_T("numSelected for %s.\n"), obj->OiList->name);
	if (!_tcscmp(obj->OiList->name, _T("DarkScript object")))
		DebugBreak();
	// If not selected (i.e. filtered) by conditions, default to all selected.
	// SelectedInOR is true even when event has no OR.
	if (poil->EventCount != obj->AdRunHeader->rh2.EventCount)
	{
		LOGI(_T("Event %i has event count/unselected. Expected NObjects: %i.\n"), DarkEdif::GetEventNumber(obj->AdRunHeader->EventGroup), poil->NObjects);
		return poil->NObjects;
	}

	// Filtered OR messes up NumOfSelected, in a bug dating back to Fusion 2.0 and until CF2.5 b294 (Aug 2022) at least.
	// Both the Select Object and Scope Control rely on NumOfSelected to be correct, though.
	// Logical OR doesn't mess up NumOfSelected.
	// OrInGroup flag applies for both filtered and logical OR, but OrLogical only applies for logical.
	// Worth noting if you combine both types of ORs in one event, the one that is first chooses the flags;
	// so unfortunately we can't tell if a logical OR indicates the lack of a filtered OR in the same event,
	// so we have to consider the OrInGroup as invalidating NumOfSelected.
	if ((obj->AdRunHeader->EventGroup->evgFlags & EventGroupFlags::OrInGroup) != EventGroupFlags::OrInGroup)
	{
		LOGI(_T("OR not found in event %i. Expecting %i.\n"), DarkEdif::GetEventNumber(obj->AdRunHeader->EventGroup), poil->NumOfSelected);
		return poil->NumOfSelected;
	}
	// OR event, and not filtered/selected
	else if (!obj->SelectedInOR)
		return poil->NObjects;

	int i = 0;
	for (short num = poil->ListSelected; num >= 0; num = (obj->AdRunHeader->ObjectList + num)->oblOffset->NextSelected)
		++i;
	LOGI(_T("Event %i may have filtered OR. List selected expected: %i. Calculated: %i.\n"), DarkEdif::GetEventNumber(obj->AdRunHeader->EventGroup), poil->NumOfSelected, i);
	return i;
}

int numSelectedQualifier(Extension * ext, int qualID)
{
	int instanceCount = 0;
	for (int i = 0; i < ext->rhPtr->NumberOi; ++i)
	{
		objInfoList* oil = (objInfoList*)(((char*)ext->rhPtr->OiList) + ext->Runtime.ObjectSelection.oiListItemSize * i);
		for (size_t j = 0; j < std::size(oil->Qualifiers); j++) {
			if (oil->Qualifiers[j] == -1)
				break;

			if (oil->Qualifiers[j] != qualID)
				continue;
			short num = oil->Object;
			if (num != -1)
				instanceCount += numSelected((ext->rdPtr->rHo.AdRunHeader->ObjectList + num)->oblOffset);
		}
	}

	return instanceCount;
}

bool inTestFunc;
const TCHAR* Extension::TestFunc(int fixedValue)
{
	str.str(_T(""s));

	auto runObj = Runtime.RunObjPtrFromFixed(fixedValue);
	int originalSelection = numSelected(&runObj->roHo);
	origEventCount = rhPtr->rh2.EventCount;
	origEventCountOR = rhPtr->rh4.EventCountOR;

	str << _T("Started with "sv) << originalSelection <<
		_T(", rh2 event count = "sv) << origEventCount <<
		_T(", rh4 eventCountOR = "sv) << rhPtr->rh4.EventCountOR << _T(".\n"sv);
	LOGI(_T("* %s"), str.str().c_str());

	/*
	inTestFunc = true;
	auto runObj = Runtime.RunObjPtrFromFixed(fixedValue);
	origEventCount = rhPtr->rh2.EventCount;
	origEventCountOR = rhPtr->rh4.EventCountOR;
	int originalSelection = numSelected(&runObj->roHo);

	::fixedValue = fixedValue;
	str << _T("Started with "sv) << originalSelection <<
		_T(", rh2 event count = "sv) << origEventCount << 
		_T(", rh4 eventCountOR = "sv) << rhPtr->rh4.EventCountOR << _T(".\n"sv);
	evt_SaveSelectedObjects(selectedObjects);
	Runtime.GenerateEvent(2);

	str << _T("Ended up with [orig] ") << numSelected(&runObj->roHo) <<
		_T(", rh2 event count = "sv) << rhPtr->rh2.EventCount <<
		_T(", rh4 eventCountOR = "sv) << rhPtr->rh4.EventCountOR << _T(".\n"sv);

	evt_RestoreSelectedObjects(selectedObjects, true);

	int newEventCount = rhPtr->rh2.EventCount;
	//runObj->roHo.OiList->EventCount = origEventCount;
	//rhPtr->rh2.EventCount = origEventCount;
	
	int newSelection = numSelected(&runObj->roHo);
	str << _T("Ended up with [reset] ") << newSelection <<
		_T(", rh2 event count = "sv) << newEventCount <<
		_T(", rh4 eventCountOR = "sv) << rhPtr->rh4.EventCountOR << _T(".\n"sv);

	inTestFunc = false;
	return Runtime.CopyString(str.str().c_str());*/
	return Runtime.CopyString(str.str().c_str());
}