/*
 * Copyright (C) 2006-2010 - Frictional Games
 *
 * This file is part of HPL1 Engine.
 *
 * HPL1 Engine is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HPL1 Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HPL1 Engine.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <filesystem>
#include <system_error>
#include "resources/Resources.h"

#include "resources/FileSearcher.h"
#include "resources/GpuProgramManager.h"
#include "resources/ParticleManager.h"
#include "resources/SoundManager.h"
#include "resources/FontManager.h"
#include "resources/ScriptManager.h"
#include "resources/TextureManager.h"
#include "resources/MaterialManager.h"
#include "resources/MeshManager.h"
#include "resources/MeshLoaderHandler.h"
#include "resources/SoundEntityManager.h"
#include "resources/AnimationManager.h"
#include "resources/ConfigFile.h"
#include "resources/LanguageFile.h"
#include "resources/impl/MeshLoaderGLTF2.h"
#include "resources/impl/MeshLoaderCollada.h"
#include "sound/Sound.h"

#include "system/Log.h"
#include "system/Files.h"

#include "tinyXML/tinyxml.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cResources::cResources()
	{
		mpDefaultEntity3DLoader = NULL;
		mpDefaultArea3DLoader = NULL;

		mpLanguageFile = NULL;

		AddBaseDirectories();
	}

	//-----------------------------------------------------------------------

	cResources::~cResources()
	{
		Log("Exiting Resources Module\n");
		Log("--------------------------------------------------------\n");

		STLMapDeleteAll(m_mEntity3DLoaders);
		STLMapDeleteAll(m_mArea3DLoaders);

		delete mpFontManager;
		delete mpScriptManager;
		delete mpParticleManager;
		delete mpSoundManager;
		delete mpMeshManager;
		delete mpMaterialManager;
		delete mpSoundEntityManager;
		delete mpAnimationManager;

		Log(" All resources deleted\n");

		delete mpMeshLoaderHandler;

		if(mpLanguageFile) delete mpLanguageFile;

		Log("--------------------------------------------------------\n\n");
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cResources::Init(iLowLevelGraphics *llGfx, cGraphicsDrawer* drawer, cTextureManager *textureMgr, cGpuProgramManager *shaderMgr, cSound *apSound, cScript *apScript, cScene *apScene)
	{
		Log(" Creating resource managers\n");
		Log("--------------------------------------------------------\n");

		mpMaterialManager = new cMaterialManager(llGfx, textureMgr, shaderMgr);
		mpParticleManager = new cParticleManager(llGfx, mpMaterialManager);
		mpFontManager = new cFontManager(textureMgr, drawer);

		mpScriptManager = new cScriptManager(apScript);

		mpSoundManager = new cSoundManager(apSound->GetLowLevel());
		mpSoundEntityManager = new cSoundEntityManager(apSound);

		// [Rehatched]: this should not be in here, shared by mesh/anim loads
		mpMeshLoaderHandler = new cMeshLoaderHandler(this, apScene);
		mpMeshLoaderHandler->AddLoader(new cMeshLoaderGLTF2(llGfx));
		mpMeshLoaderHandler->AddLoader(new cMeshLoaderCollada(llGfx));

		mpMeshManager = new cMeshManager(mpMeshLoaderHandler);
		mpAnimationManager = new cAnimationManager(mpMeshLoaderHandler);

		Log("--------------------------------------------------------\n\n");
	}

	//-----------------------------------------------------------------------

	bool cResources::LoadResourceDirsFile(const tString &asFile)
	{
		TiXmlDocument* pXmlDoc = new TiXmlDocument(asFile.c_str());
		if(pXmlDoc->LoadFile()==false)
		{
			Error("Couldn't load XML file '%s'!\n",asFile.c_str());
			delete  pXmlDoc;
			return false;
		}

		//Get the root.
		TiXmlElement* pRootElem = pXmlDoc->RootElement();

		TiXmlElement* pChildElem = pRootElem->FirstChildElement();
		for(; pChildElem != NULL; pChildElem = pChildElem->NextSiblingElement())
		{
			tString sPath = cString::ToString(pChildElem->Attribute("Path"),"");
			if(sPath==""){
				continue;
			}

			if(sPath[0]=='/' || sPath[0]=='\\') sPath = sPath.substr(1);

			AddResourceDir(sPath);
		}

		delete pXmlDoc;
		return true;
	}

	/**
	 * \todo File searcher should check so if the dir is allready added and if so return false and not add
	 * \param &asDir
	 * \param &asMask
	 * \return
	 */
	bool cResources::AddResourceDir(const tString &asDir, const tString &asMask)
	{
		FileSearcher::AddDirectory(asDir, asMask);
		if(iResourceBase::GetLogCreateAndDelete())
			Log(" Added resource directory '%s'\n",asDir.c_str());
		return true;
	}

	void cResources::AddBaseDirectories() {
		// core graphics files
		AddResourceDir("core/programs");
		AddResourceDir("core/textures");

		// rehatched core graphics overrides
		AddResourceDir("rehatched/core/programs");
		AddResourceDir("rehatched/core/textures");
	}

	/**
	 * Reset resource directory search paths to built-ins + game specific
	 * paths specified in a resources.cfg file.
	 * Ideally resource paths would be grouped and individually reset
	 * but that is not the HPL1 way.
	 */
	void cResources::SetupResourceDirsForLanguage(const tString &asLangFile)
	{
		FileSearcher::ClearDirectories();
		AddBaseDirectories();
		LoadResourceDirsFile("resources.cfg");
		LoadResourceDirsFile("rehatched/resources.cfg");
		SetLanguageFile(asLangFile);
	}

	//-----------------------------------------------------------------------

	/**
	 * Finds a file in a directory whatever case it was asked for.
	 *
	 * The game asks for "english.lang" by default while the file shipped with
	 * it is "English.lang". That works on the exFAT card a handheld usually
	 * has, and fails on any case sensitive filesystem, where the language file
	 * is not found, SetLanguageFile gives up, the resource directories for the
	 * language are never registered, and every font and texture after it fails
	 * too. Two Retroid Pocket 5 testers saw that as a black screen with sound
	 * and then a crash, while the same card contents worked on an RG353V.
	 */
	static tString ResolveIgnoringCase(const tString &asDir, const tString &asFile)
	{
		const tString sExact = asDir + asFile;
		if(FileExists(cString::To16Char(sExact))) return sExact;

		std::error_code err;
		for(const auto &entry : std::filesystem::directory_iterator(asDir, err))
		{
			if(err) break;
			const tString sName = entry.path().filename().string();
			if(cString::ToLowerCase(sName) == cString::ToLowerCase(asFile))
				return asDir + sName;
		}
		return "";
	}

	//-----------------------------------------------------------------------

	bool cResources::SetLanguageFile(const tString &asFile)
	{
		// [ZM] made the decision to not use the file searcher for language files.
		// /config was already the de-facto only position for them and with resource
		// overloading this makes path handling setup less awkward.
		tString sOrigPath = ResolveIgnoringCase("config/", asFile);
		tString sRehatchedPath = ResolveIgnoringCase("rehatched/config/", asFile);
		if(sRehatchedPath.empty()) sRehatchedPath = "rehatched/config/" + asFile;

		if (sOrigPath.empty())
		{
			Error("Couldn't find language file '%s' in config/\n",asFile.c_str());
			return false;
		}
		if (FileExists(cString::To16Char(sRehatchedPath)) == false)
		{
			sRehatchedPath = "rehatched/config/English.lang";
			if(FileExists(cString::To16Char(sRehatchedPath)) == false) {
				Error("Couldn't load language extensions file '%s'\n", sRehatchedPath.c_str());
				return false;
			}
			
			Warning("No localised language extensions file found for language '%s', using English fallback\n", asFile.c_str());
		}

		cLanguageFile *pNewLangFile = new cLanguageFile(this);

		bool bSuccess = pNewLangFile->LoadFromFile(sOrigPath);
		if (bSuccess==false) {
			delete pNewLangFile;
			return false;
		}
		bSuccess = pNewLangFile->LoadFromFile(sRehatchedPath);
		if (bSuccess==false) {
			delete pNewLangFile;
			return false;
		}

		// everything loaded, exchange old for new
		if(mpLanguageFile){
			delete mpLanguageFile;
		}
		mpLanguageFile = pNewLangFile;

		return true;
	}

	const tWString& cResources::Translate(const tString& asCat, const tString& asName)
	{
		if(mpLanguageFile)
		{
			return mpLanguageFile->Translate(asCat,asName);
		}
		else
		{
			return mwsEmptyString;
		}
	}

	//-----------------------------------------------------------------------

	void cResources::AddEntity3DLoader(iEntity3DLoader* apLoader, bool abSetAsDefault)
	{
		m_mEntity3DLoaders.insert(tEntity3DLoaderMap::value_type(apLoader->GetName(), apLoader));

		if(abSetAsDefault){
			mpDefaultEntity3DLoader = apLoader;
		}
	}

	iEntity3DLoader* cResources::GetEntity3DLoader(const tString& asName)
	{
		tEntity3DLoaderMapIt it = m_mEntity3DLoaders.find(asName);
		if(it == m_mEntity3DLoaders.end()){
			Warning("No loader for type '%s' found!\n",asName.c_str());

			if(mpDefaultEntity3DLoader){
				Log("Using default loader!\n");
				return mpDefaultEntity3DLoader;
			}
			else {
				return NULL;
			}
		}

		return it->second;
	}

	//-----------------------------------------------------------------------


	void cResources::AddArea3DLoader(iArea3DLoader* apLoader, bool abSetAsDefault)
	{
		m_mArea3DLoaders.insert(tArea3DLoaderMap::value_type(apLoader->GetName(), apLoader));

		if(abSetAsDefault){
			mpDefaultArea3DLoader = apLoader;
		}
	}

	iArea3DLoader* cResources::GetArea3DLoader(const tString& asName)
	{
		tArea3DLoaderMapIt it = m_mArea3DLoaders.find(asName);
		if(it == m_mArea3DLoaders.end()){
			Warning("No loader for area type '%s' found!\n",asName.c_str());

			if(mpDefaultArea3DLoader){
				Log("Using default loader!\n");
				return mpDefaultArea3DLoader;
			}
			else {
				return NULL;
			}
		}

		return it->second;
	}

	//-----------------------------------------------------------------------

}
