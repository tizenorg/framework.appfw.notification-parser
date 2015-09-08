/*
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include <stdio.h>
#include <errno.h>
#include <sqlite3.h>
#include <db-util.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <dlog.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pkgmgr-info.h>

#if !defined(FLOG)
#define DbgPrint(format, arg...)	LOGD("[[32m%s/%s[0m:%d] " format, basename(__FILE__), __func__, __LINE__, ##arg)
#define ErrPrint(format, arg...)	LOGE("[[32m%s/%s[0m:%d] " format, basename(__FILE__), __func__, __LINE__, ##arg)
#endif

#if defined(LOG_TAG)
#undef LOG_TAG
#endif

#define LOG_TAG "PKGMGR_NOTIFICATION"

/*!
 * \note
 * DB Table schema
 *
 * notification_setting
 * +-------+--------------+---------+----------+-------+-------+-----------+-----------+
 * | appid | notification | sounds  | contents | badge | pkgid | reserved1 | reserved2 |
 * +-------+--------------+---------+----------+-------+-------+-----------+-----------+
 * |   -   |      -       |    -    |     -    |   -   |   -   |     -     |     -     |
 * +-------+--------------+---------+----------+-------+-------+-----------+-----------+
 *
 * CREATE TABLE notification_setting ( appid TEXT PRIMARY KEY NOT NULL, notification TEXT, sounds TEXT, contents TEXT, badge TEXT, pkgid TEXT, reserved1 TEXT, reserved2 TEXT )
 *
*/


///////////////////////////////////////////////////////////////////////////////////////////////////
// DATABASE
///////////////////////////////////////////////////////////////////////////////////////////////////
#define DB_PATH "/opt/usr/dbspace/.notification_parser.db"

static struct {
	const char* pPath;
	sqlite3* pHandle;
} databaseInfo = {
	.pPath = DB_PATH,
	.pHandle = NULL
};

struct notification_setting {
	xmlChar* pAppId;
	xmlChar* pNotification;
	xmlChar* pSounds;
	xmlChar* pContents;
	xmlChar* pBadge;
	char* pPkgId;
	xmlChar* pReserved1;
	xmlChar* pReserved2;
};

static inline int begin_transaction(void)
{
	sqlite3_stmt* pStmt = NULL;

	int ret = sqlite3_prepare_v2(databaseInfo.pHandle, "BEGIN TRANSACTION", -1, &pStmt, NULL);
	if (ret != SQLITE_OK)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		return EXIT_FAILURE;
	}

	if (sqlite3_step(pStmt) != SQLITE_DONE)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		sqlite3_finalize(pStmt);
		return EXIT_FAILURE;
	}

	sqlite3_finalize(pStmt);
	return EXIT_SUCCESS;
}

static inline int rollback_transaction(void)
{
	sqlite3_stmt* pStmt = NULL;

	int ret = sqlite3_prepare_v2(databaseInfo.pHandle, "ROLLBACK TRANSACTION", -1, &pStmt, NULL);
	if (ret != SQLITE_OK)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		return EXIT_FAILURE;
	}

	if (sqlite3_step(pStmt) != SQLITE_DONE) 
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		sqlite3_finalize(pStmt);
		return EXIT_FAILURE;
	}

	sqlite3_finalize(pStmt);
	return EXIT_SUCCESS;
}

static inline int commit_transaction(void)
{
	sqlite3_stmt* pStmt = NULL;

	int ret = sqlite3_prepare_v2(databaseInfo.pHandle, "COMMIT TRANSACTION", -1, &pStmt, NULL);
	if (ret != SQLITE_OK)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		return EXIT_FAILURE;
	}

	if (sqlite3_step(pStmt) != SQLITE_DONE)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		sqlite3_finalize(pStmt);
		return EXIT_FAILURE;
	}

	sqlite3_finalize(pStmt);
	return EXIT_SUCCESS;
}

static inline int db_create_notification_setting(void)
{
	char* pException = NULL;
	static const char* pQuery = "CREATE TABLE notification_setting ( appid TEXT PRIMARY KEY NOT NULL, notification TEXT, sounds TEXT, contents TEXT, badge TEXT, pkgid TEXT, reserved1 TEXT, reserved2 TEXT )";

	if (sqlite3_exec(databaseInfo.pHandle, pQuery, NULL, NULL, &pException) != SQLITE_OK)
	{
		ErrPrint("%s\n", pException);
		return -EIO;
	}

	if (sqlite3_changes(databaseInfo.pHandle) == 0)
	{
		DbgPrint("The database is not changed.\n");
	}

	return 0;
}

static inline int db_insert_notification_setting(struct notification_setting* pNotiSetting)
{
	sqlite3_stmt* pStmt = NULL;
	static const char* pQuery = "INSERT INTO notification_setting (appid, notification, sounds, contents, badge, pkgid, reserved1, reserved2) VALUES (? ,?, ?, ?, ?, ?, null, null)";

	if (pNotiSetting == NULL)
	{
		ErrPrint("pNotiSetting is NULL.\n");
		return -EINVAL;
	}

	int ret = sqlite3_prepare_v2(databaseInfo.pHandle, pQuery, -1, &pStmt, NULL);
	if (ret != SQLITE_OK)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		return -EIO;
	}

	ret = sqlite3_bind_text(pStmt, 1, (char*)pNotiSetting->pAppId, -1, SQLITE_TRANSIENT);
	if (ret != SQLITE_OK)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		ret = -EIO;
		goto out;
	}

	ret = sqlite3_bind_text(pStmt, 2, (char*)pNotiSetting->pNotification, -1, SQLITE_TRANSIENT);
	if (ret != SQLITE_OK)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		ret = -EIO;
		goto out;
	}

	ret = sqlite3_bind_text(pStmt, 3, (char*)pNotiSetting->pSounds, -1, SQLITE_TRANSIENT);
	if (ret != SQLITE_OK)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		ret = -EIO;
		goto out;
	}

	ret = sqlite3_bind_text(pStmt, 4, (char*)pNotiSetting->pContents, -1, SQLITE_TRANSIENT);
	if (ret != SQLITE_OK)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		ret = -EIO;
		goto out;
	}

	ret = sqlite3_bind_text(pStmt, 5, (char*)pNotiSetting->pBadge, -1, SQLITE_TRANSIENT);
	if (ret != SQLITE_OK)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		ret = -EIO;
		goto out;
	}
	
	ret = sqlite3_bind_text(pStmt, 6, pNotiSetting->pPkgId, -1, SQLITE_TRANSIENT);
	if (ret != SQLITE_OK)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		ret = -EIO;
		goto out;
	}

	ret = 0;
	if (sqlite3_step(pStmt) != SQLITE_DONE)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		ret = -EIO;
	}

out:
	sqlite3_reset(pStmt);
	sqlite3_clear_bindings(pStmt);
	sqlite3_finalize(pStmt);
	return ret;
}


static inline int db_delete_notification_setting(const char* pAppId)
{
	sqlite3_stmt* pStmt = NULL;
	static const char* pQuery = "DELETE FROM notification_setting WHERE appid = ?";

	int ret = sqlite3_prepare_v2(databaseInfo.pHandle, pQuery, -1, &pStmt, NULL);
	if (ret != SQLITE_OK)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		return -EIO;
	}

	ret = sqlite3_bind_text(pStmt, 1, pAppId, -1, SQLITE_TRANSIENT);
	if (ret != SQLITE_OK)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		ret = -EIO;
		goto out;
	}

	ret = 0;
	if (sqlite3_step(pStmt) != SQLITE_DONE)
	{
		ErrPrint("Failed to execute sqlite3_step(%s).\n", sqlite3_errmsg(databaseInfo.pHandle));
		ret = -EIO;
	}

out:
	sqlite3_reset(pStmt);
	sqlite3_clear_bindings(pStmt);
	sqlite3_finalize(pStmt);
	return ret;
}

static inline void db_create_table(void)
{
	begin_transaction();

	int ret = db_create_notification_setting();
	if (ret < 0)
	{
		rollback_transaction();
		return;
	}

	commit_transaction();
}

static inline int db_init(void)
{
	struct stat stat;

	int ret = db_util_open(databaseInfo.pPath, &databaseInfo.pHandle, DB_UTIL_REGISTER_HOOK_METHOD);
	if (ret != SQLITE_OK)
	{
		ErrPrint("%d\n", ret);
		return -EIO;
	}

	if (lstat(databaseInfo.pPath, &stat) < 0)
	{
		ErrPrint("%s\n", strerror(errno));
		db_util_close(databaseInfo.pHandle);
		databaseInfo.pHandle = NULL;
		return -EIO;
	}

	if (!S_ISREG(stat.st_mode))
	{
		ErrPrint("S_ISREG failed.\n");
		db_util_close(databaseInfo.pHandle);
		databaseInfo.pHandle = NULL;
		return -EINVAL;
	}

	if (!stat.st_size)
	{
		ErrPrint("The RPM file has not been installed properly.");
		db_create_table();
	}

	return 0;
}

static inline int db_connect(void)
{
	if (!databaseInfo.pHandle)
	{
		int ret = db_init();
		if (ret < 0)
		{
			ErrPrint("%d\n", ret);
			return -EIO;
		}
	}

	return 0;
}

static inline int db_disconnect(void)
{
	if (!databaseInfo.pHandle)
	{
		return 0;
	}

	db_util_close(databaseInfo.pHandle);
	databaseInfo.pHandle = NULL;

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PLUGIN - INSTALL
///////////////////////////////////////////////////////////////////////////////////////////////////
int PKGMGR_PARSER_PLUGIN_PRE_INSTALL(const char* pPkgId)
{
	DbgPrint("PKGMGR_PARSER_PLUGIN_PRE_INSTALL(pPkgId: %s)\n", pPkgId);
	return 0;
}

void release_memory(struct notification_setting* pNotiSetting)
{
	if (pNotiSetting == NULL)
	{
		return;
	}

	if (pNotiSetting->pAppId)
	{
		xmlFree(pNotiSetting->pAppId);
	}
	if (pNotiSetting->pNotification)
	{
		xmlFree(pNotiSetting->pNotification);
	}
	if (pNotiSetting->pSounds)
	{
		xmlFree(pNotiSetting->pSounds);
	}
	if (pNotiSetting->pContents)
	{
		xmlFree(pNotiSetting->pContents);
	}
	if (pNotiSetting->pBadge)
	{
		xmlFree(pNotiSetting->pBadge);
	}

	//don't need to delete it
	//pNotiSetting->pPkgId

	if (pNotiSetting->pReserved1)
	{
		xmlFree(pNotiSetting->pReserved1);
	}
	if (pNotiSetting->pReserved2)
	{
		xmlFree(pNotiSetting->pReserved2);
	}

	free(pNotiSetting);
}

int notification_install_data(xmlDocPtr docPtr, const char* pPkgId)
{
	if (db_connect() < 0)
	{
		return -EIO;
	}

	xmlNodePtr node = xmlFirstElementChild(xmlDocGetRootElement(docPtr));
	if (node == NULL)
	{
		ErrPrint("xmlDocGetRootElement failed.\n");
		db_disconnect();
		return -EINVAL;
	}

	struct notification_setting* pNotiSetting = calloc(1, sizeof(struct notification_setting));
	if (pNotiSetting == NULL)
	{
		ErrPrint("out of memory.\n");
		xmlFreeDoc(docPtr);
		db_disconnect();
		return -ENOMEM;
	}

	pNotiSetting->pAppId = xmlGetProp(node, (const xmlChar*)"appid");
	if (pNotiSetting->pAppId == NULL)
	{
		ErrPrint("xmlGetProp failed.\n");
		xmlFreeDoc(docPtr);
		free(pNotiSetting);
		db_disconnect();
		return -EINVAL;
	}

	for (node = node->children; node; node = node->next)
	{
		if (!xmlStrcmp(node->name, (const xmlChar*)"notification"))
		{
			xmlChar* pSection = xmlGetProp(node, (const xmlChar*)"section");

			if (!xmlStrcmp(pSection, (const xmlChar*)"notification"))
			{
				pNotiSetting->pNotification = xmlNodeGetContent(node);
			}
			else if (!xmlStrcmp(pSection, (const xmlChar*)"sounds"))
			{
				pNotiSetting->pSounds = xmlNodeGetContent(node);
			}
			else if (!xmlStrcmp(pSection, (const xmlChar*)"contents"))
			{
				pNotiSetting->pContents = xmlNodeGetContent(node);
			}
			else if (!xmlStrcmp(pSection, (const xmlChar*)"badge"))
			{
				pNotiSetting->pBadge = xmlNodeGetContent(node);
			}

			xmlFree(pSection);
		}
	}

	// set default value
	if (pNotiSetting->pNotification == NULL)
	{
		pNotiSetting->pNotification = xmlCharStrdup("on");
	}
	if (pNotiSetting->pSounds == NULL)
	{
		pNotiSetting->pSounds = xmlCharStrdup("on");
	}
	if (pNotiSetting->pContents == NULL)
	{
		pNotiSetting->pContents = xmlCharStrdup("off");
	}
	if (pNotiSetting->pBadge == NULL)
	{
		pNotiSetting->pBadge = xmlCharStrdup("on");
	}

	pNotiSetting->pPkgId = (char*)pPkgId;

	int ret = db_insert_notification_setting(pNotiSetting);
	DbgPrint("The result of executing db_insert_notification_setting for (%s) : %d\n", (char*)pNotiSetting->pAppId, ret);

	release_memory(pNotiSetting);
	xmlFreeDoc(docPtr);

	db_disconnect();

	return 0;
}

int PKGMGR_PARSER_PLUGIN_INSTALL(xmlDocPtr docPtr, const char* pPkgId)
{
	DbgPrint("PKGMGR_PARSER_PLUGIN_INSTALL(pPkgId: %s)\n", pPkgId);
	return notification_install_data(docPtr, pPkgId);
}

int PKGMGR_PARSER_PLUGIN_POST_INSTALL(const char* pPkgId)
{
	DbgPrint("PKGMGR_PARSER_PLUGIN_POST_INSTALL(pPkgId: %s)\n", pPkgId);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PLUGIN - UPGRADE
///////////////////////////////////////////////////////////////////////////////////////////////////

static inline int notification_delete_data(const char* pPkgId)
{
	sqlite3_stmt* pStmt = NULL;
	static const char* pQuery = "DELETE FROM notification_setting WHERE appid in (SELECT appid FROM notification_setting WHERE pkgid = ?)";

	int ret = sqlite3_prepare_v2(databaseInfo.pHandle, pQuery, -1, &pStmt, NULL);
	if (ret != SQLITE_OK)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		return -EIO;
	}

	ret = sqlite3_bind_text(pStmt, 1, pPkgId, -1, SQLITE_TRANSIENT);
	if (ret != SQLITE_OK)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		ret = -EIO;
		goto out;
	}

	ret = 0;
	if (sqlite3_step(pStmt) != SQLITE_DONE)
	{
		ErrPrint("Failed to execute sqlite3_step(%s).\n", sqlite3_errmsg(databaseInfo.pHandle));
		ret = -EIO;
	}

out:
	sqlite3_reset(pStmt);
	sqlite3_clear_bindings(pStmt);
	sqlite3_finalize(pStmt);
	return ret;
}

int PKGMGR_PARSER_PLUGIN_PRE_UPGRADE(const char* pPkgId)
{
	DbgPrint("PKGMGR_PARSER_PLUGIN_PRE_UPGRADE(pPkgId: %s)\n", pPkgId);

	if (db_connect() < 0)
	{
		return -EIO;
	}

	sqlite3_stmt* pStmt = NULL;

	static const char* pQuery = "SELECT COUNT(*) FROM notification_setting WHERE pkgid = ?";
	int ret = sqlite3_prepare_v2(databaseInfo.pHandle, pQuery, -1, &pStmt, NULL);
	if (ret != SQLITE_OK)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		return -EIO;
	}

	ret = sqlite3_bind_text(pStmt, 1, pPkgId, -1, SQLITE_TRANSIENT);
	if (ret != SQLITE_OK)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		ret = -EIO;
		goto out;
	}

	ret = 0;
	if (sqlite3_step(pStmt) != SQLITE_ROW)
	{
		ErrPrint("%s\n", sqlite3_errmsg(databaseInfo.pHandle));
		ret = -EIO;
		goto out;
	}

	int count = sqlite3_column_int(pStmt, 0);
	DbgPrint("%d app existed in %s\n", count, pPkgId);

	if (count > 0)
	{
		ret = notification_delete_data(pPkgId);
		DbgPrint("The result of executing notification_delete_data(%d)\n", ret);
	}

out:
	sqlite3_reset(pStmt);
	sqlite3_clear_bindings(pStmt);
	sqlite3_finalize(pStmt);
	db_disconnect();
	return ret;
}

int PKGMGR_PARSER_PLUGIN_UPGRADE(xmlDocPtr docPtr, const char* pPkgId)
{
	DbgPrint("PKGMGR_PARSER_PLUGIN_UPGRADE(pPkgId: %s)\n", pPkgId);
	return notification_install_data(docPtr, pPkgId);
}

int PKGMGR_PARSER_PLUGIN_POST_UPGRADE(const char* pPkgId)
{
	DbgPrint("PKGMGR_PARSER_PLUGIN_POST_UPGRADE(pPkgId: %s)\n", pPkgId);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PLUGIN - UNINSTALL
///////////////////////////////////////////////////////////////////////////////////////////////////
int PKGMGR_PARSER_PLUGIN_PRE_UNINSTALL(const char* pPkgId)
{
	DbgPrint("PKGMGR_PARSER_PLUGIN_PRE_UNINSTALL(pPkgId: %s)\n", pPkgId);
	return 0;
}

int app_func(pkgmgrinfo_appinfo_h handle, void *user_data)
{
	char* pAppId = NULL;
	pkgmgrinfo_appinfo_get_appid(handle, &pAppId);

	int ret = db_delete_notification_setting(pAppId);
	DbgPrint("The result of executing db_delete_notification_setting for (%s) : %d\n", pAppId, ret);

	return 0;
}

int PKGMGR_PARSER_PLUGIN_UNINSTALL(xmlDocPtr docPtr, const char* pPkgId)
{
	DbgPrint("PKGMGR_PARSER_PLUGIN_UNINSTALL(pPkgId: %s)\n", pPkgId);

	pkgmgrinfo_pkginfo_h handle;
	int ret = pkgmgrinfo_pkginfo_get_pkginfo(pPkgId, &handle);
	if (ret != PMINFO_R_OK)
	{
		ErrPrint("%d\n", ret);
		return -EINVAL;
	}

	if (db_connect() < 0)
	{
		ret = -EIO;
		goto out;
	}

	ret = pkgmgrinfo_appinfo_get_list(handle, PMINFO_ALL_APP, app_func, NULL);
	if (ret != PMINFO_R_OK)
	{
		ErrPrint("%d\n", ret);
	}

	db_disconnect();

out:
	pkgmgrinfo_pkginfo_destroy_pkginfo(handle);
	return ret;
}

int PKGMGR_PARSER_PLUGIN_POST_UNINSTALL(const char* pPkgId)
{
	DbgPrint("PKGMGR_PARSER_PLUGIN_POST_UNINSTALL(pPkgId: %s)\n", pPkgId);
	return 0;
}
