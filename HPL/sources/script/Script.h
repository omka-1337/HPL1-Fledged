/*
 * 2021 by zenmumbler
 * This file is part of Rehatched
 */
#ifndef HPL_SCRIPTSYSTEM_H
#define HPL_SCRIPTSYSTEM_H

#include "game/Updateable.h"
#include "resources/ResourceBase.h"

#include <map>

class asIScriptEngine;
class asIScriptModule;
class asIScriptContext;
class asIScriptFunction;

namespace hpl {

	class cResources;
	class cScriptOutput;

	class cScriptModule : public iResourceBase {
	public:
		cScriptModule(const tString& asName,asIScriptEngine *apScriptEngine, cScriptOutput *apScriptOutput);
		~cScriptModule();

		bool CreateFromFile(const tString& asFile);

		/**
		 * Runs a func in the script, for example "test(15)"
		 * \param asFuncLine the line of code
		 * \return true if everything was ok, else false
		 */
		bool Run(const tString& asFuncLine);

	private:
		asIScriptFunction* FindCachedFunc(const tString& asName);

		asIScriptEngine *mpEngine;
		cScriptOutput *mpOutput;
		asIScriptModule *mpModule;
		asIScriptContext *mpContext;

		// Resolved once per name; a NULL value means "not a module function",
		// which is cached too so the lookup is not repeated every frame.
		std::map<tString, asIScriptFunction*> m_mapFuncCache;
	};

	class cScript : public iUpdateable
	{
	public:
		cScript();
		~cScript();

		void Init();

		void Update(float afTimeStep);

		/**
		 * Creates a ne script
		 * \param asName name of the script.
		 * \return
		 */
		cScriptModule* CreateScript(const tString& asName);

		/**
		 * Add a function to the script vm. Example: "void test(float x)"
		 * \param asFuncDecl the declaration.
		 * \return
		 */
		bool AddScriptFunc(const tString& asFuncDecl, void* pFunc);

		/**
		 * Add a variable to the script vm. Example: "int MyVar"
		 * \param asVarDecl the declartion
		 * \param *pVar the variable
		 * \return
		 */
		bool AddScriptVar(const tString& asVarDecl, void *pVar);

		asIScriptEngine* GetEngine() const { return mpEngine; };

	private:
		asIScriptEngine *mpEngine;
		cScriptOutput *mpOutput;
	};

};

#endif // HPL_SCRIPTSYSTEM_H
