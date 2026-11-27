/*
	This file is part of Warzone 2100.
	Copyright (C) 2005-2020  Warzone 2100 Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
#include "frame.h"
#include "string_ext.h"

#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <cstring>
#include <cstdint>
#include <unordered_map>

// Android builds ship without libintl (ENABLE_NLS=OFF). Use a small built-in .mo reader instead,
// so the language option works: see "Built-in message catalog support" below.
#if defined(__ANDROID__) && !defined(ENABLE_NLS)
# define WZ_BUILTIN_MO 1
#endif

#include <locale.h>
#include <physfs.h>
#include "wzpaths.h"

#ifdef WZ_OS_MAC
# include <CoreFoundation/CoreFoundation.h>
# include <CoreFoundation/CFURL.h>
#endif

#include <nonstd/optional.hpp>
using nonstd::optional;
using nonstd::nullopt;

/* Always use fallbacks on Windows */
#if defined(WZ_OS_WIN)
#  undef LOCALEDIR
#endif

#if !defined(LOCALEDIR)
#  define LOCALEDIR "locale"
#endif

#ifdef PORTABLE
#  ifdef PACKAGE
#   undef PACKAGE
#   define PACKAGE "warzone2100_portable"
#  endif
#endif
// Language names (http://en.wikipedia.org/wiki/List_of_ISO_639-2_codes)
#define LANG_NAME_ARABIC "العربية"
#define LANG_NAME_BASQUE "Euskara"
#define LANG_NAME_BULGARIAN "български език"
#define LANG_NAME_CATALAN "Català"
#define LANG_NAME_CHINESE_SIMPLIFIED "汉语"
#define LANG_NAME_CHINESE_TRADITIONAL "漢語"
#define LANG_NAME_CROATIAN "Hrvatski"
#define LANG_NAME_CZECH "Česky"
#define LANG_NAME_DANISH "Dansk"
#define LANG_NAME_DUTCH "Nederlands"
#define LANG_NAME_ENGLISH "English"
#define LANG_NAME_ENGLISH_UK "British English"
#define LANG_NAME_ESTONIAN "Eesti Keel"
#define LANG_NAME_FINNISH "Suomi"
#define LANG_NAME_FRENCH "Français"
#define LANG_NAME_FRISIAN_NETHERLANDS "Frysk"
#define LANG_NAME_GERMAN "Deutsch"
#define LANG_NAME_GREEK "Ελληνικά"
#define LANG_NAME_HUNGARIAN "Magyar"
#define LANG_NAME_INDONESIAN "Bahasa Indonesia"
#define LANG_NAME_IRISH "Imruagadh"
#define LANG_NAME_ITALIAN "Italiano"
#define LANG_NAME_JAPANESE "日本語"
#define LANG_NAME_KOREAN "한국어"
#define LANG_NAME_LATIN "Latine"
#define LANG_NAME_LATVIAN "Latviešu valoda"
#define LANG_NAME_LITHUANIAN "Lietuvių kalba"
#define LANG_NAME_NORWEGIAN "Norsk"
#define LANG_NAME_NORWEGIAN_NYNORSK "Nynorsk"
#define LANG_NAME_POLISH "Polski"
#define LANG_NAME_PORTUGUESE_BRAZILIAN "Português Brasileiro"
#define LANG_NAME_PORTUGUESE "Português"
#define LANG_NAME_ROMANIAN "Română"
#define LANG_NAME_RUSSIAN "Русский"
#define LANG_NAME_SERBIAN "Srpski"
#define LANG_NAME_SLOVAK "Slovensky"
#define LANG_NAME_SLOVENIAN "Slovenski"
#define LANG_NAME_SPANISH "Español"
#define LANG_NAME_SWEDISH "Svenska"
#define LANG_NAME_SWEDISH_SWEDEN "Svenska (Sverige)"
#define LANG_NAME_TURKISH "Türkçe"
#define LANG_NAME_UKRAINIAN "Українська"
#define LANG_NAME_UZBEK_CYRILLIC "Ўзбек"

#if defined(WZ_OS_WIN)
/*
 *  See msdn.microsoft.com for this stuff, esp.
 *  http://msdn.microsoft.com/en-us/library/ms693062%28VS.85,printer%29.aspx
 *  http://msdn.microsoft.com/en-us/library/dd318693%28VS.85,printer%29.aspx
 */
static const struct
{
	const char *language;
	const char *name;
	const char *localeFilename; // the actual filename on disk, minus the extension (ex. "en_GB")
	USHORT usPrimaryLanguage;
	USHORT usSubLanguage;
} map[] =
{
	{ "", N_("System locale"), "", LANG_NEUTRAL, SUBLANG_DEFAULT },
#  if defined(ENABLE_NLS) || defined(WZ_BUILTIN_MO)
	{ "ar_SA", LANG_NAME_ARABIC, "ar_SA", LANG_ARABIC, SUBLANG_DEFAULT },
	{ "bg", LANG_NAME_BULGARIAN, "bg_BG", LANG_BULGARIAN, SUBLANG_DEFAULT },
	{ "ca_ES", LANG_NAME_CATALAN, "ca_ES", LANG_CATALAN, SUBLANG_DEFAULT },
	{ "cs_CZ", LANG_NAME_CZECH, "cs", LANG_CZECH, SUBLANG_DEFAULT },
	{ "da", LANG_NAME_DANISH, "da", LANG_DANISH, SUBLANG_DEFAULT },
	{ "de", LANG_NAME_GERMAN, "de", LANG_GERMAN, SUBLANG_GERMAN },
	{ "el_GR", LANG_NAME_GREEK, "el", LANG_GREEK, SUBLANG_DEFAULT },
	{ "en", LANG_NAME_ENGLISH, "en", LANG_ENGLISH, SUBLANG_DEFAULT },
	{ "en_GB", LANG_NAME_ENGLISH_UK, "en_GB", LANG_ENGLISH, SUBLANG_ENGLISH_UK },
	{ "es", LANG_NAME_SPANISH, "es", LANG_SPANISH, SUBLANG_SPANISH },
	{ "et_EE", LANG_NAME_ESTONIAN, "et_EE", LANG_ESTONIAN, SUBLANG_DEFAULT },
//	{ "eu", LANG_NAME_BASQUE, LANG_BASQUE, SUBLANG_DEFAULT },
	{ "fi", LANG_NAME_FINNISH, "fi", LANG_FINNISH, SUBLANG_DEFAULT },
	{ "fr", LANG_NAME_FRENCH, "fr", LANG_FRENCH, SUBLANG_FRENCH },
	/* Our Frisian translation is the "West Frisian" variation of it. This
	 * variation is mostly spoken in the Dutch province Friesland (Fryslân
	 * in Frisian) and has ISO 639-3 code "fry".
	 *
	 * FIXME: We should really use a sub-language code for this. E.g.
	 *        fy_XX.
	 */
	{ "fy_NL", LANG_NAME_FRISIAN_NETHERLANDS, "fy", LANG_FRISIAN, SUBLANG_FRISIAN_NETHERLANDS },
	{ "ga_IE", LANG_NAME_IRISH, "ga", LANG_IRISH, SUBLANG_IRISH_IRELAND },
	{ "hr", LANG_NAME_CROATIAN, "hr", LANG_CROATIAN, SUBLANG_DEFAULT },
	{ "hu", LANG_NAME_HUNGARIAN, "hu", LANG_HUNGARIAN, SUBLANG_DEFAULT },
	{ "id", LANG_NAME_INDONESIAN, "id", LANG_INDONESIAN, SUBLANG_DEFAULT },
	{ "it", LANG_NAME_ITALIAN, "it", LANG_ITALIAN, SUBLANG_ITALIAN },
	{ "ko_KR", LANG_NAME_KOREAN, "ko", LANG_KOREAN, SUBLANG_DEFAULT },
//	{ "la", LANG_NAME_LATIN, "la", LANG_LATIN, SUBLANG_DEFAULT },
	{ "lt", LANG_NAME_LITHUANIAN, "lt", LANG_LITHUANIAN, SUBLANG_DEFAULT },
//	{ "lv", LANG_NAME_LATVIAN, LANG_LATVIAN, SUBLANG_DEFAULT },
	// MSDN uses "no"...
	{ "nb_NO", LANG_NAME_NORWEGIAN, "nb", LANG_NORWEGIAN, SUBLANG_DEFAULT },
//	{ "nn", LANG_NAME_NORWEGIAN_NYNORSK, LANG_NORWEGIAN, SUBLANG_NORWEGIAN_NYNORSK },
	{ "nl", LANG_NAME_DUTCH, "nl", LANG_DUTCH, SUBLANG_DUTCH },
	{ "pl", LANG_NAME_POLISH, "pl", LANG_POLISH, SUBLANG_DEFAULT },
	{ "pt_BR", LANG_NAME_PORTUGUESE_BRAZILIAN, "pt_BR", LANG_PORTUGUESE, SUBLANG_PORTUGUESE_BRAZILIAN },
	{ "pt", LANG_NAME_PORTUGUESE, "pt", LANG_PORTUGUESE, SUBLANG_DEFAULT },
	{ "ro", LANG_NAME_ROMANIAN, "ro", LANG_ROMANIAN, SUBLANG_DEFAULT },
	{ "ru", LANG_NAME_RUSSIAN, "ru", LANG_RUSSIAN, SUBLANG_DEFAULT },
	{ "sk", LANG_NAME_SLOVAK, "sk", LANG_SLOVAK, SUBLANG_DEFAULT },
	{ "sl_SI", LANG_NAME_SLOVENIAN, "sl", LANG_SLOVENIAN, SUBLANG_DEFAULT },
	{ "sr_RS", LANG_NAME_SERBIAN, "sr_CS", LANG_SERBIAN, SUBLANG_SERBIAN_LATIN }, // Due to how gettext-libintl maps Win32 LangIDs to locales, use sr_CS for Windows
#if (WINVER >= 0x0600)
//	{ "sv_SE", LANG_NAME_SWEDISH_SWEDEN, LANG_SWEDISH, SUBLANG_SWEDISH_SWEDEN },
#else
//	{ "sv_SE", LANG_NAME_SWEDISH_SWEDEN, LANG_SWEDISH, SUBLANG_SWEDISH },
#endif
//	{ "sv", LANG_NAME_SWEDISH, LANG_SWEDISH, SUBLANG_DEFAULT },
	{ "tr", LANG_NAME_TURKISH, "tr", LANG_TURKISH, SUBLANG_DEFAULT },
//	{ "uz", LANG_NAME_UZBEK_CYRILLIC, LANG_UZBEK, SUBLANG_UZBEK_CYRILLIC },
	{ "uk_UA", LANG_NAME_UKRAINIAN, "uk_UA", LANG_UKRAINIAN, SUBLANG_DEFAULT },
	{ "zh_CN", LANG_NAME_CHINESE_SIMPLIFIED, "zh_CN", LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED },
	{ "zh_TW", LANG_NAME_CHINESE_TRADITIONAL, "zh_TW", LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL },
#  endif
};
#else
static const struct
{
	const char *language;
	const char *name;
	const char *localeFilename; // the actual filename on disk, minus the extension (ex. "en_GB")
	const char *locale;
	const char *localeFallback;
} map[] =
{
	{ "",   N_("System locale"), "", "", "" },
#  if defined(ENABLE_NLS) || defined(WZ_BUILTIN_MO)
	{ "ar_SA", LANG_NAME_ARABIC, "ar_SA", "ar_SA.UTF-8", "ar"},
	{ "bg", LANG_NAME_BULGARIAN, "bg_BG", "bg_BG.UTF-8", "bg" },
	{ "ca_ES", LANG_NAME_CATALAN, "ca_ES", "ca_ES.UTF-8", "ca" },
	{ "cs_CZ", LANG_NAME_CZECH, "cs", "cs_CZ.UTF-8", "cs" },
	{ "da", LANG_NAME_DANISH, "da", "da_DK.UTF-8", "da_DK" },
	{ "de", LANG_NAME_GERMAN, "de", "de_DE.UTF-8", "de_DE" },
	{ "el_GR", LANG_NAME_GREEK, "el", "el_GR.UTF-8", "el" },
	{ "en", LANG_NAME_ENGLISH, "en", "en_US.UTF-8", "en_US" },
	{ "en_GB", LANG_NAME_ENGLISH_UK, "en_GB", "en_GB.UTF-8", "en_GB" },
	{ "es", LANG_NAME_SPANISH, "es", "es_ES.UTF-8", "es_ES" },
	{ "et_EE", LANG_NAME_ESTONIAN, "et_EE", "et_EE.UTF-8", "et" },
//	{ "eu_ES", LANG_NAME_BASQUE, "eu_ES.UTF-8", "eu" },
	{ "fi", LANG_NAME_FINNISH, "fi", "fi_FI.UTF-8", "fi_FI" },
	{ "fr", LANG_NAME_FRENCH, "fr", "fr_FR.UTF-8", "fr_FR" },
	/* Our Frisian translation is the "West Frisian" variation of it. This
	 * variation is mostly spoken in the Dutch province Friesland (Fryslân
	 * in Frisian) and has ISO 639-3 code "fry".
	 */
	{ "fy_NL", LANG_NAME_FRISIAN_NETHERLANDS, "fy", "fy_NL.UTF-8", "fy" },
	{ "ga_IE", LANG_NAME_IRISH, "ga", "ga_IE.UTF-8", "ga" },
	{ "hr", LANG_NAME_CROATIAN, "hr", "hr_HR.UTF-8", "hr_HR" },
	{ "hu", LANG_NAME_HUNGARIAN, "hu", "hu_HU.UTF-8", "hu_HU" },
	{ "id", LANG_NAME_INDONESIAN, "id", "id_ID.UTF-8", "id" },
	{ "it", LANG_NAME_ITALIAN, "it", "it_IT.UTF-8", "it_IT" },
	{ "ja_JP", LANG_NAME_JAPANESE, "ja_JP", "ja_JP.UTF-8", "ja" },
	{ "ko_KR", LANG_NAME_KOREAN, "ko", "ko_KR.UTF-8", "ko" },
	{ "la", LANG_NAME_LATIN, "la", "la.UTF-8", "la" },
	{ "lt", LANG_NAME_LITHUANIAN, "lt", "lt_LT.UTF-8", "lt_LT" },
//	{ "lv", LANG_NAME_LATVIAN, "lv_LV.UTF-8", "lv_LV" },
	{ "nb_NO", LANG_NAME_NORWEGIAN, "nb", "nb_NO.UTF-8", "nb" },
//	{ "nn_NO", LANG_NAME_NORWEGIAN_NYNORSK, "nn_NO.UTF-8", "nn" },
	{ "nl", LANG_NAME_DUTCH, "nl", "nl_NL.UTF-8", "nl_NL" },
	{ "pl", LANG_NAME_POLISH, "pl", "pl_PL.UTF-8", "pl_PL" },
	{ "pt_BR", LANG_NAME_PORTUGUESE_BRAZILIAN, "pt_BR", "pt_BR.UTF-8", "pt_BR" },
	{ "pt", LANG_NAME_PORTUGUESE, "pt", "pt_PT.UTF-8", "pt_PT" },
	{ "ro", LANG_NAME_ROMANIAN, "ro", "ro_RO.UTF-8", "ro_RO" },
	{ "ru", LANG_NAME_RUSSIAN, "ru", "ru_RU.UTF-8", "ru_RU" },
	{ "sk", LANG_NAME_SLOVAK, "sk", "sk_SK.UTF-8", "sk_SK" },
	{ "sl_SI", LANG_NAME_SLOVENIAN, "sl", "sl_SI.UTF-8", "sl" },
	{ "sr_RS", LANG_NAME_SERBIAN, "sr_RS", "sr_RS.UTF-8@latin", "sr_RS" }, // Use the newer "sr_RS" identifier (with optional @latin)
//	{ "sv_SE", LANG_NAME_SWEDISH_SWEDEN, "sv_SE.UTF-8", "sv" },
//	{ "sv", LANG_NAME_SWEDISH, "sv.UTF-8", "sv" },
	{ "tr", LANG_NAME_TURKISH, "tr", "tr_TR.UTF-8", "tr_TR" },
//	{ "uz", LANG_NAME_UZBEK_CYRILLIC, "uz_UZ.UTF-8", "uz_UZ" },
	{ "uk_UA", LANG_NAME_UKRAINIAN, "uk_UA", "uk_UA.UTF-8", "uk" },
	{ "zh_CN", LANG_NAME_CHINESE_SIMPLIFIED, "zh_CN", "zh_CN.UTF-8", "zh_CN" },
	{ "zh_TW", LANG_NAME_CHINESE_TRADITIONAL, "zh_TW", "zh_TW.UTF-8", "zh_TW" },
#  endif
};
#endif

static unsigned int selectedLanguage = 0;

#if defined(WZ_BUILTIN_MO)
// MARK: - Built-in message catalog support (Android)

namespace
{

static inline uint32_t wzSwap32(uint32_t v)
{
	return ((v & 0xFFu) << 24) | ((v & 0xFF00u) << 8) | ((v >> 8) & 0xFF00u) | ((v >> 24) & 0xFFu);
}

// Tiny evaluator for the C-like expression in a catalog's "Plural-Forms: ... plural=EXPR;" header
struct PluralEval
{
	const char *p;
	unsigned long n;
	bool err = false;

	PluralEval(const char *expr, unsigned long nValue) : p(expr), n(nValue) {}

	void ws() { while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') { ++p; } }

	long ternary()
	{
		long c = orExpr();
		ws();
		if (*p == '?')
		{
			++p;
			long a = ternary();
			ws();
			if (*p == ':') { ++p; } else { err = true; }
			long b = ternary();
			return c ? a : b;
		}
		return c;
	}
	long orExpr()
	{
		long v = andExpr();
		for (;;)
		{
			ws();
			if (p[0] == '|' && p[1] == '|') { p += 2; long r = andExpr(); v = (v || r); }
			else { return v; }
		}
	}
	long andExpr()
	{
		long v = eqExpr();
		for (;;)
		{
			ws();
			if (p[0] == '&' && p[1] == '&') { p += 2; long r = eqExpr(); v = (v && r); }
			else { return v; }
		}
	}
	long eqExpr()
	{
		long v = relExpr();
		for (;;)
		{
			ws();
			if (p[0] == '=' && p[1] == '=') { p += 2; long r = relExpr(); v = (v == r); }
			else if (p[0] == '!' && p[1] == '=') { p += 2; long r = relExpr(); v = (v != r); }
			else { return v; }
		}
	}
	long relExpr()
	{
		long v = addExpr();
		for (;;)
		{
			ws();
			if (p[0] == '<' && p[1] == '=') { p += 2; long r = addExpr(); v = (v <= r); }
			else if (p[0] == '>' && p[1] == '=') { p += 2; long r = addExpr(); v = (v >= r); }
			else if (p[0] == '<') { ++p; long r = addExpr(); v = (v < r); }
			else if (p[0] == '>') { ++p; long r = addExpr(); v = (v > r); }
			else { return v; }
		}
	}
	long addExpr()
	{
		long v = mulExpr();
		for (;;)
		{
			ws();
			if (*p == '+') { ++p; v += mulExpr(); }
			else if (*p == '-') { ++p; v -= mulExpr(); }
			else { return v; }
		}
	}
	long mulExpr()
	{
		long v = unary();
		for (;;)
		{
			ws();
			if (*p == '*') { ++p; v *= unary(); }
			else if (*p == '/') { ++p; long r = unary(); if (r == 0) { err = true; return 0; } v /= r; }
			else if (*p == '%') { ++p; long r = unary(); if (r == 0) { err = true; return 0; } v %= r; }
			else { return v; }
		}
	}
	long unary()
	{
		ws();
		if (*p == '!') { ++p; return !unary(); }
		if (*p == '(')
		{
			++p;
			long v = ternary();
			ws();
			if (*p == ')') { ++p; } else { err = true; }
			return v;
		}
		if (*p == 'n') { ++p; return static_cast<long>(n); }
		if (*p >= '0' && *p <= '9')
		{
			long v = 0;
			while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); ++p; }
			return v;
		}
		err = true;
		return 0;
	}
};

// One loaded .mo file. The original-string table in a .mo is sorted, so lookups are a binary search over the file image.
struct MoCatalog
{
	std::vector<char> data;
	uint32_t count = 0;
	uint32_t origTable = 0;
	uint32_t transTable = 0;
	bool swapped = false;
	unsigned nplurals = 2;
	std::string pluralExpr; // empty => English-like rule (n != 1)

	uint32_t rd(size_t off) const
	{
		uint32_t v;
		memcpy(&v, data.data() + off, sizeof(v));
		return swapped ? wzSwap32(v) : v;
	}

	bool parse()
	{
		if (data.size() < 28) { return false; }
		uint32_t magic;
		memcpy(&magic, data.data(), sizeof(magic));
		if (magic == 0x950412deu) { swapped = false; }
		else if (magic == 0xde120495u) { swapped = true; }
		else { return false; }
		if ((rd(4) >> 16) != 0) { return false; } // unsupported major revision
		count = rd(8);
		origTable = rd(12);
		transTable = rd(16);
		const uint64_t size = data.size();
		if (static_cast<uint64_t>(origTable) + static_cast<uint64_t>(count) * 8 > size) { return false; }
		if (static_cast<uint64_t>(transTable) + static_cast<uint64_t>(count) * 8 > size) { return false; }

		// Header entry (msgid "") carries the plural rule
		size_t idx = 0;
		if (findIndex("", idx))
		{
			const char *header = stringAt(transTable, idx, nullptr);
			if (header)
			{
				const char *pf = strstr(header, "Plural-Forms:");
				if (pf)
				{
					const char *np = strstr(pf, "nplurals=");
					if (np) { nplurals = static_cast<unsigned>(strtoul(np + 9, nullptr, 10)); }
					const char *pl = strstr(pf, "plural=");
					if (pl)
					{
						pl += 7;
						const char *end = pl;
						while (*end && *end != ';' && *end != '\n') { ++end; }
						pluralExpr.assign(pl, end);
					}
				}
			}
		}
		if (nplurals == 0 || nplurals > 8) { nplurals = 2; }
		return true;
	}

	// Returns a pointer to string `index` of the given table, validating bounds + NUL terminator
	const char *stringAt(uint32_t table, size_t index, uint32_t *lenOut) const
	{
		const size_t entry = static_cast<size_t>(table) + index * 8;
		const uint32_t len = rd(entry);
		const uint32_t off = rd(entry + 4);
		if (static_cast<uint64_t>(off) + len + 1 > data.size()) { return nullptr; }
		if (data[static_cast<size_t>(off) + len] != '\0') { return nullptr; }
		if (lenOut) { *lenOut = len; }
		return data.data() + off;
	}

	bool findIndex(const char *key, size_t &indexOut) const
	{
		size_t lo = 0, hi = count;
		while (lo < hi)
		{
			const size_t mid = lo + (hi - lo) / 2;
			const char *orig = stringAt(origTable, mid, nullptr);
			if (!orig) { return false; }
			const int cmp = strcmp(key, orig);
			if (cmp == 0) { indexOut = mid; return true; }
			if (cmp < 0) { hi = mid; } else { lo = mid + 1; }
		}
		return false;
	}

	// Singular lookup (also used for context keys: "ctx\004msgid"). Returns the first translated form or nullptr.
	const char *lookup(const char *key) const
	{
		size_t idx;
		if (!findIndex(key, idx)) { return nullptr; }
		const char *t = stringAt(transTable, idx, nullptr);
		return (t && t[0] != '\0') ? t : nullptr;
	}

	unsigned pluralIndex(unsigned long n) const
	{
		long result = (n != 1) ? 1 : 0;
		if (!pluralExpr.empty())
		{
			PluralEval ev(pluralExpr.c_str(), n);
			const long v = ev.ternary();
			if (!ev.err && v >= 0) { result = v; }
		}
		if (result >= static_cast<long>(nplurals)) { result = static_cast<long>(nplurals) - 1; }
		return static_cast<unsigned>(result);
	}

	const char *lookupPlural(const char *key, unsigned long n) const
	{
		size_t idx;
		if (!findIndex(key, idx)) { return nullptr; }
		uint32_t len = 0;
		const char *t = stringAt(transTable, idx, &len);
		if (!t) { return nullptr; }
		unsigned form = pluralIndex(n);
		const char *cur = t;
		const char *end = t + len;
		while (form > 0 && cur < end)
		{
			cur += strlen(cur) + 1;
			--form;
		}
		if (cur > end) { return nullptr; }
		return (cur[0] != '\0') ? cur : nullptr;
	}
};

// Catalogs are never freed once loaded: callers may keep `const char *` results from gettext (like libintl allows).
static std::mutex catalogMutex;
static std::map<std::string, std::unique_ptr<MoCatalog>> loadedCatalogs; // key: path (null value = known-missing)
static const MoCatalog *activeMainCatalog = nullptr;
static const MoCatalog *activeGuideCatalog = nullptr;

static const MoCatalog *loadMoFile(const std::string &path)
{
	auto it = loadedCatalogs.find(path);
	if (it != loadedCatalogs.end() && it->second)
	{
		return it->second.get();
	}
	if (!PHYSFS_isInit() || !PHYSFS_exists(path.c_str()))
	{
		return nullptr; // may become available later (search paths are set up after the config is first loaded)
	}
	PHYSFS_file *file = PHYSFS_openRead(path.c_str());
	if (!file)
	{
		return nullptr;
	}
	const PHYSFS_sint64 length = PHYSFS_fileLength(file);
	auto catalog = std::make_unique<MoCatalog>();
	if (length > 0 && length < (64 * 1024 * 1024))
	{
		catalog->data.resize(static_cast<size_t>(length));
		const PHYSFS_sint64 got = WZ_PHYSFS_readBytes(file, catalog->data.data(), static_cast<PHYSFS_uint32>(length));
		if (got != length) { catalog->data.clear(); }
	}
	PHYSFS_close(file);
	if (catalog->data.empty() || !catalog->parse())
	{
		debug(LOG_ERROR, "Failed to read message catalog: %s", path.c_str());
		return nullptr;
	}
	const MoCatalog *result = catalog.get();
	loadedCatalogs[path] = std::move(catalog);
	return result;
}

// ---------------------------------------------------------------------------------------------
// Warzone2100 Mobile: "System locale" follows the language of the phone.
// The preferred languages of the device (most preferred first, e.g. "ar_SA", "fr_FR", "en_US") are
// provided by the platform layer through wzI18nSetSystemLocales().
// ---------------------------------------------------------------------------------------------
static std::vector<std::string> systemLocaleTags;

static std::string wzLowerAscii(std::string s)
{
	for (auto &c : s) { if (c >= 'A' && c <= 'Z') { c = static_cast<char>(c - 'A' + 'a'); } }
	return s;
}

// Returns: > 0 index into map[], 0 = English (use the built-in strings), -1 = no match for this tag
static int findLanguageIndexForTag(const std::string &rawTag)
{
	std::string tag = rawTag;
	const size_t cut = tag.find_first_of(".@");
	if (cut != std::string::npos) { tag.resize(cut); }
	for (auto &c : tag) { if (c == '-') { c = '_'; } }

	// split into language / script / region
	std::vector<std::string> parts;
	size_t start = 0;
	while (start <= tag.size())
	{
		const size_t us = tag.find('_', start);
		parts.push_back(tag.substr(start, us == std::string::npos ? std::string::npos : us - start));
		if (us == std::string::npos) { break; }
		start = us + 1;
	}
	if (parts.empty() || parts[0].empty()) { return -1; }
	std::string lang = wzLowerAscii(parts[0]);
	std::string region;
	bool traditionalScript = false;
	for (size_t i = 1; i < parts.size(); ++i)
	{
		if (parts[i].size() == 4 && wzLowerAscii(parts[i]) == "hant") { traditionalScript = true; }
		if (parts[i].size() == 2)
		{
			region = parts[i];
			for (auto &c : region) { if (c >= 'a' && c <= 'z') { c = static_cast<char>(c - 'a' + 'A'); } }
		}
	}

	// Android / ISO aliases
	if (lang == "in") { lang = "id"; }
	else if (lang == "no" || lang == "nn") { lang = "nb"; }
	else if (lang == "iw") { lang = "he"; }
	else if (lang == "tl") { lang = "fil"; }

	if (lang == "en") { return 0; } // English strings are built in
	if (lang == "zh")
	{
		// Traditional Chinese for Taiwan / Hong Kong / Macau (or a Hant script tag); simplified otherwise
		region = (traditionalScript || region == "TW" || region == "HK" || region == "MO") ? "TW" : "CN";
	}

	auto matches = [](const char *candidate, const std::string &wanted) {
		return candidate && candidate[0] != '\0' && wzLowerAscii(candidate) == wanted;
	};
	const std::string full = region.empty() ? lang : (lang + "_" + wzLowerAscii(region));

	// 1) exact match with the full language_REGION tag
	if (!region.empty())
	{
		for (unsigned int i = 1; i < ARRAY_SIZE(map); ++i)
		{
			if (matches(map[i].language, full) || matches(map[i].localeFilename, full) || matches(map[i].localeFallback, full))
			{
				return static_cast<int>(i);
			}
		}
	}
	// 2) the plain language, written exactly ("pt", "de", "cs")
	for (unsigned int i = 1; i < ARRAY_SIZE(map); ++i)
	{
		if (matches(map[i].language, lang) || matches(map[i].localeFilename, lang))
		{
			return static_cast<int>(i);
		}
	}
	// 3) any entry of the same language ("ar" -> "ar_SA", "cs" -> "cs_CZ")
	const std::string prefix = lang + "_";
	for (unsigned int i = 1; i < ARRAY_SIZE(map); ++i)
	{
		const std::string language = wzLowerAscii(map[i].language ? map[i].language : "");
		const std::string filename = wzLowerAscii(map[i].localeFilename ? map[i].localeFilename : "");
		if (language.compare(0, prefix.size(), prefix) == 0 || filename.compare(0, prefix.size(), prefix) == 0)
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}

static unsigned int resolveLanguageIndex(unsigned int languageIdx)
{
	if (languageIdx != 0) { return languageIdx; }
	for (const std::string &tag : systemLocaleTags) // caller holds catalogMutex
	{
		const int idx = findLanguageIndexForTag(tag);
		if (idx >= 0) { return static_cast<unsigned int>(idx); } // 0 = English: stop here, it is the preferred language
	}
	return 0;
}

static void activateCatalogsForLanguage(unsigned int requestedLanguageIdx)
{
	std::lock_guard<std::mutex> lock(catalogMutex);
	activeMainCatalog = nullptr;
	activeGuideCatalog = nullptr;
	const unsigned int languageIdx = resolveLanguageIndex(requestedLanguageIdx);
	if (languageIdx == 0 || languageIdx >= ARRAY_SIZE(map))
	{
		return; // English: original strings
	}

	// Directory names tried (matches the usual gettext layout: locale/<name>/LC_MESSAGES/<domain>.mo)
	std::vector<std::string> names;
	names.push_back(map[languageIdx].localeFilename);
	names.push_back(map[languageIdx].language);
	std::string fallback = map[languageIdx].locale;
	const size_t dot = fallback.find('.');
	if (dot != std::string::npos) { fallback.resize(dot); }
	if (!fallback.empty()) { names.push_back(fallback); }

	for (const auto &name : names)
	{
		const std::string base = "locale/" + name + "/LC_MESSAGES/";
		if (!activeMainCatalog) { activeMainCatalog = loadMoFile(base + "warzone2100.mo"); }
		if (!activeGuideCatalog) { activeGuideCatalog = loadMoFile(base + "warzone2100_guide.mo"); }
		if (activeMainCatalog && activeGuideCatalog) { break; }
	}
	debug(LOG_WZ, "Message catalogs for \"%s\": main=%s guide=%s", map[languageIdx].language,
		activeMainCatalog ? "loaded" : "none", activeGuideCatalog ? "loaded" : "none");
}

static const MoCatalog *catalogForDomain(const char *domain)
{
	std::lock_guard<std::mutex> lock(catalogMutex);
	if (domain)
	{
		const size_t len = strlen(domain);
		if (len >= 6 && strcmp(domain + len - 6, "_guide") == 0)
		{
			return activeGuideCatalog;
		}
	}
	return activeMainCatalog;
}

} // anonymous namespace

namespace
{

// English strings about the mouse / clicks / function keys, reworded for a phone (tap, long-press, touch).
// Used only when the current language has no translation for the string (see wzI18nGettext below).
struct PhoneTerm
{
	const char *original;
	const char *phone;
};
static const PhoneTerm kPhoneTerms[] = {
#include "i18n_phone_terms.inc"
};

static const char *phoneTermFor(const char *msgid)
{
	static const std::unordered_map<std::string, const char *> terms = []() {
		std::unordered_map<std::string, const char *> map;
		for (const PhoneTerm &term : kPhoneTerms)
		{
			map.emplace(term.original, term.phone);
		}
		return map;
	}();
	const auto it = terms.find(msgid);
	return it != terms.end() ? it->second : nullptr;
}

static bool isGuideDomain(const char *domain)
{
	if (!domain) { return false; }
	const size_t len = strlen(domain);
	return len >= 6 && strcmp(domain + len - 6, "_guide") == 0;
}

} // anonymous namespace

extern "C" const char *wzI18nGettext(const char *domain, const char *msgid)
{
	if (!msgid || msgid[0] == '\0') { return msgid; }
	const MoCatalog *catalog = catalogForDomain(domain);
	const char *translated = catalog ? catalog->lookup(msgid) : nullptr;
	if (translated) { return translated; }
	if (!isGuideDomain(domain))
	{
		// No translation (e.g. English): use phone wording instead of mouse / click wording
		if (const char *phone = phoneTermFor(msgid)) { return phone; }
	}
	return msgid;
}

extern "C" const char *wzI18nNgettext(const char *domain, const char *msgid1, const char *msgid2, unsigned long n)
{
	const char *untranslated = (n == 1) ? msgid1 : msgid2;
	if (!msgid1 || msgid1[0] == '\0') { return untranslated; }
	const MoCatalog *catalog = catalogForDomain(domain);
	if (!catalog) { return untranslated; }
	const char *translated = catalog->lookupPlural(msgid1, n);
	return translated ? translated : untranslated;
}

void wzI18nSetSystemLocales(const std::vector<std::string> &tags)
{
	{
		std::lock_guard<std::mutex> lock(catalogMutex);
		systemLocaleTags = tags;
	}
	for (const std::string &tag : tags)
	{
		debug(LOG_WZ, "System language preference: %s", tag.c_str());
	}
}

void wzI18nReloadCatalogs()
{
	activateCatalogsForLanguage(selectedLanguage);
}
#else
void wzI18nSetSystemLocales(const std::vector<std::string> &)
{
	// no-op: only the built-in .mo reader (Android) needs the device language list
}

void wzI18nReloadCatalogs()
{
	// no-op: catalogs are managed by libintl (or NLS is disabled)
}
#endif // WZ_BUILTIN_MO
static bool canUseLANGUAGEEnvVar = false;

static std::string compileDate;

/*!
 * Return the language part of the selected locale
 */
#if defined(WZ_BUILTIN_MO)
const char *getLanguage()
{
	return map[selectedLanguage].language;
}
#elif !defined(ENABLE_NLS)
const char *getLanguage()
{
	return "";
}
#elif defined(WZ_OS_WIN)
const char *getLanguage()
{
	USHORT usPrimaryLanguage = PRIMARYLANGID(LANGIDFROMLCID(GetThreadLocale()));
	unsigned int i;

	if (selectedLanguage == 0)
	{
		return "";  // Return empty string for system default
	}

	for (i = 0; i < ARRAY_SIZE(map); i++)
	{
		if (usPrimaryLanguage == map[i].usPrimaryLanguage)
		{
			return map[i].language;
		}
	}

	return "";
}
#else
const char *getLanguage()
{
	static char language[6] = { '\0' }; // large enough for xx_YY
	const char *localeName = setlocale(LC_MESSAGES, NULL);
	char *delim = NULL;

	if (selectedLanguage == 0 || localeName == NULL)
	{
		return "";  // Return empty string for system default and errors
	}

	sstrcpy(language, localeName);

	// cut anything after a '.' to get rid of the encoding part
	delim = strchr(language, '.');
	if (delim)
	{
		*delim = '\0';
	}

	// if language is xx_XX, cut the _XX part
	delim = strchr(language, '_');
	if (delim)
	{
		if (!strncasecmp(language, delim + 1, 2))
		{
			*delim = '\0';
		}
	}

	return language;
}
#endif


const char *getLanguageName()
{
	const char *language = getLanguage();
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(map); i++)
	{
		if (strcmp(language, map[i].language) == 0)
		{
			return gettext(map[i].name);
		}
	}

	return language;
}


#if defined(ENABLE_NLS)
#  if defined(WZ_OS_WIN)
static bool setLocaleWindows(USHORT usPrimaryLanguage, USHORT usSubLanguage)
{
	bool success = SUCCEEDED(SetThreadLocale(MAKELCID(MAKELANGID(usPrimaryLanguage, usSubLanguage), SORT_DEFAULT)));

	if (!success)
	{
		wz_info("Failed to set locale to \"%d\"", usPrimaryLanguage);
	}
	else
	{
		debug(LOG_WZ, "Requested locale \"%d\"", usPrimaryLanguage);
	}

	setlocale(LC_NUMERIC, "C"); // set radix character to the period (".")

	return success;
}
#  elif defined(__EMSCRIPTEN__)
static bool setLocaleEmscripten(const char *localeFilename)
{
	setlocale(LC_ALL, localeFilename);

	const char * numericLocale = setlocale(LC_NUMERIC, "C"); // set radix character to the period (".")
	debug(LOG_WZ, "LC_NUMERIC: \"%s\"", (numericLocale != nullptr) ? numericLocale : "<null>");

	return true;
}
#  else
/*!
 * Set the prefered locale
 * \param locale The locale, NOT just the language part
 * \note Use this instead of setlocale(), because we need the default radix character
 */
static bool setLocaleUnix(const char *locale)
{
	const char *pEnvLang = nullptr;

	// Set the appropriate language environment variables *before* the call to libintl's `setlocale`.
	// (The LANGUAGE and LANG env-vars) (temporarily saving the old values)
	// This is *REQUIRED* on some platforms where other environment variables (LC_ALL, LC_MESSAGES, etc) are sometimes set (ex. macOS), or run-time language switching won't work
	pEnvLang = getenv("LANGUAGE");
	std::string prior_LANGUAGE((pEnvLang != nullptr) ? pEnvLang : "");
	pEnvLang = getenv("LANG");
	std::string prior_LANG((pEnvLang != nullptr) ? pEnvLang : "");
#if defined(HAVE_SETENV)
	setenv("LANGUAGE", locale, 1);
	setenv("LANG", locale, 1);
#else
	# warning "No supported method to set environment variables"
#endif

	const char *actualLocale = setlocale(LC_ALL, locale);

	if (actualLocale == NULL)
	{
		wz_info("Failed to set locale to \"%s\"", locale);
#if defined(HAVE_SETENV)
		setenv("LANGUAGE", prior_LANGUAGE.c_str(), 1);
		setenv("LANG", prior_LANG.c_str(), 1);
#else
		# warning "No supported method to set environment variables"
#endif
	}
	else
	{
		if (strcmp(locale, actualLocale))
		{
			debug(LOG_WZ, "Requested locale \"%s\", got \"%s\" instead", locale, actualLocale);
		}
	}

	const char * numericLocale = setlocale(LC_NUMERIC, "C"); // set radix character to the period (".")
	debug(LOG_WZ, "LC_NUMERIC: \"%s\"", (numericLocale != nullptr) ? numericLocale : "<null>");

	return (actualLocale != NULL);
}
static bool setLocaleUnix_LANGUAGEFallback(const char *localeFilename)
{
	if (!canUseLANGUAGEEnvVar)
	{
		return false;
	}
#if defined(HAVE_SETENV) // or possibly other methods in the future?
	const char *pEnvLang = getenv("LANGUAGE");
	std::string prior_LANGUAGE((pEnvLang != nullptr) ? pEnvLang : "");

	setlocale(LC_ALL, ""); // use default system locale

#if defined(HAVE_SETENV)
	setenv("LANGUAGE", localeFilename, 1); // libintl will pick this up and use this preferentially
#else
	# warning "No supported method to set environment variables"
#endif

	const char * numericLocale = setlocale(LC_NUMERIC, "C"); // set radix character to the period (".")
	debug(LOG_WZ, "LC_NUMERIC: \"%s\"", (numericLocale != nullptr) ? numericLocale : "<null>");

	return true;
#else // defined(HAVE_SETENV)
	return false;
#endif // defined(HAVE_SETENV)
}
#  endif
#endif


bool setLanguage(const char *language)
{
#if defined(WZ_BUILTIN_MO)
	for (unsigned int i = 0; i < ARRAY_SIZE(map); i++)
	{
		if (strcmp(language, map[i].language) == 0)
		{
			selectedLanguage = i;
			debug(LOG_WZ, "Setting language to \"%s\" (%s)", map[i].name, map[i].language);
			activateCatalogsForLanguage(i); // if search paths aren't ready yet, wzI18nReloadCatalogs() retries later
			return true;
		}
	}
	debug(LOG_ERROR, "Requested language \"%s\" not supported.", language);
	return false;
#elif !defined(ENABLE_NLS)
	return true;
#else
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(map); i++)
	{
		if (strcmp(language, map[i].language) == 0)
		{
			selectedLanguage = i;
			debug(LOG_WZ, "Setting language to \"%s\" (%s)", map[i].name, map[i].language);

#  if defined(WZ_OS_WIN)
			return setLocaleWindows(map[i].usPrimaryLanguage, map[i].usSubLanguage);
#  elif defined(__EMSCRIPTEN__)
			return setLocaleEmscripten(map[i].localeFilename);
#  else
			return setLocaleUnix(map[i].locale) || setLocaleUnix(map[i].localeFallback) || setLocaleUnix_LANGUAGEFallback(map[i].localeFilename);
#  endif
		}
	}

	debug(LOG_ERROR, "Requested language \"%s\" not supported.", language);

	return false;
#endif
}

std::vector<locale_info> getLocales()
{
	std::vector<locale_info> locales;
	for (const auto &entry: map)
	{
		locales.push_back({entry.language, entry.name});
	}

	return locales;
}

std::string wzBindTextDomain(const char *domainname, const char *dirname)
{
	debug(LOG_WZ, "bindTextDomain(%s, %s)", (domainname) ? domainname : "NULL", (dirname) ? dirname : "NULL");

#if (LIBINTL_VERSION >= 0x001500) && defined(_WIN32) && !defined(__CYGWIN__)
	// gettext 0.21+ provides a wbindtextdomain function on native Windows platforms
	// that properly supports Unicode paths

	// convert the dirname from UTF-8 to UTF-16
	int wstr_len = MultiByteToWideChar(CP_UTF8, 0, dirname, -1, NULL, 0);
	if (wstr_len <= 0)
	{
		DWORD dwError = GetLastError();
		debug(LOG_ERROR, "Could not not convert string from UTF-8; MultiByteToWideChar failed with error %lu: %s\n", dwError, dirname);
		return std::string();
	}
	auto wstr_dirname = std::vector<wchar_t>(wstr_len, L'\0');
	if (MultiByteToWideChar(CP_UTF8, 0, dirname, -1, &wstr_dirname[0], wstr_len) == 0)
	{
		DWORD dwError = GetLastError();
		debug(LOG_ERROR, "Could not not convert string from UTF-8; MultiByteToWideChar[2] failed with error %lu: %s\n", dwError, dirname);
		return std::string();
	}

	// and call wbindtextdomain
	const wchar_t *pResult = wbindtextdomain(domainname, wstr_dirname.data());
	if (!pResult)
	{
		debug(LOG_ERROR, "wbindtextdomain failed");
		return std::string();
	}

	// convert the result back to UTF-8
	std::vector<char> utf8Buffer;
	int utf8Len = WideCharToMultiByte(CP_UTF8, 0, pResult, -1, NULL, 0, NULL, NULL);
	if ( utf8Len <= 0 )
	{
		// Encoding conversion error
		DWORD dwError = GetLastError();
		debug(LOG_ERROR, "Could not not convert string to UTF-8; WideCharToMultiByte failed with error %lu\n", dwError);
		return std::string();
	}
	utf8Buffer.resize(utf8Len, 0);
	if ( (utf8Len = WideCharToMultiByte(CP_UTF8, 0, pResult, -1, &utf8Buffer[0], utf8Len, NULL, NULL)) <= 0 )
	{
		// Encoding conversion error
		DWORD dwError = GetLastError();
		debug(LOG_ERROR, "Could not not convert string to UTF-8; WideCharToMultiByte[2] failed with error %lu\n", dwError);
		return std::string();
	}
	return std::string(utf8Buffer.data(), utf8Len - 1);
#else
	// call the normal bindtextdomain function
	const char * pResult = bindtextdomain(domainname, dirname);
	if (!pResult)
	{
		return std::string();
	}
	return std::string(pResult);
#endif
}

static bool checkSupportsLANGUAGEenvVarOverride()
{
#if defined(_LIBINTL_H) && defined(LIBINTL_VERSION)
	// Only known to be supported by GNU Gettext runtime (libintl) behavior
	// GNU Gettext permits overriding with the "LANGUAGE" environment variable to specify the language for messages
	// As long as the current locale does not equal the "plain" C locale (i.e. just "C")

	debug(LOG_WZ, "Checking for support of LANGUAGE env var override");

# if defined(WZ_OS_WIN)
	// NOTE: Not currently supported on Windows because we take advantage of GNU Gettext's gl_locale_name fall-back to GetThreadLocale()
	//		 But if we call setlocale and set values, it won't fall-back.
	return false;

# else // !defined(WZ_OS_WIN)

	optional<std::string> prevLocale;
	const char *actualLocale = setlocale(LC_MESSAGES, NULL);
	if (actualLocale != NULL)
	{
		prevLocale = actualLocale;
	}

	// Try to set "default" locale
	actualLocale = setlocale(LC_MESSAGES, "");
	if (actualLocale == NULL)
	{
		// Failed
		return false;
	}

	// Can only use LANGUAGE override if the locale is *not* plain "C"
	bool result = !(strcmp(actualLocale, "C") == 0);

	if (prevLocale.has_value())
	{
		// reset back to prev locale before test
		setlocale(LC_MESSAGES, prevLocale.value().c_str());
	}

	debug(LOG_WZ, "Supports LANGUAGE env var override: %s", (result) ? "YES" : "no");
	return result;
# endif // defined(WZ_OS_WIN)
#else
	debug(LOG_WZ, "No support for LANGUAGE env var override (not GNU gettext?)");
	return false;
#endif // defined(_LIBINTL_H) && defined(LIBINTL_VERSION)
}

#if defined(__EMSCRIPTEN__)
static std::string getEmscriptenDefaultLanguage()
{
	auto pEnvLang = getenv("LANG");
	if (pEnvLang)
	{
		std::string defaultLanguage = pEnvLang;
		// Remove any .UTF-8 suffix
		if (strEndsWith(defaultLanguage, ".UTF-8"))
		{
			defaultLanguage = defaultLanguage.substr(0, defaultLanguage.size() - 6);
		}
		// Find matching closest language (if present)
		for (unsigned i = 0; i < ARRAY_SIZE(map); i++)
		{
			if (strcmp(defaultLanguage.c_str(), map[i].language) == 0)
			{
				return map[i].language;
			}
		}
		for (unsigned i = 0; i < ARRAY_SIZE(map); i++)
		{
			if (strcmp(defaultLanguage.c_str(), map[i].localeFilename) == 0)
			{
				return map[i].language;
			}
		}
		for (unsigned i = 0; i < ARRAY_SIZE(map); i++)
		{
			if (strcmp(defaultLanguage.c_str(), map[i].localeFallback) == 0)
			{
				return map[i].language;
			}
		}
	}
	return "";
}
#endif

static bool wzBindAllWZTextDomains(const char* resourcePath)
{
	// Bind the "main" message catalog ("warzone2100")
	auto textdomainDirectory = wzBindTextDomain(PACKAGE, resourcePath);
	(void)bind_textdomain_codeset(PACKAGE, "UTF-8");
	if (textdomainDirectory.empty())
	{
		// Failed to bind main message catalog - treat as critical failure
		return false;
	}

	// Bind the guide topics message catalog
	textdomainDirectory = wzBindTextDomain(PACKAGE "_guide", resourcePath);
	(void)bind_textdomain_codeset(PACKAGE "_guide", "UTF-8");
	if (textdomainDirectory.empty())
	{
		// Non-fatal failure
		debug(LOG_INFO, "initI18n: Failed to bind guide message catalog");
	}

	return true;
}

void initI18n()
{
	bool boundAtLeastMainTextDomain = false;
	std::string defaultLanguage = ""; // "" is system default (in most cases)

#if defined(__EMSCRIPTEN__)
	defaultLanguage = getEmscriptenDefaultLanguage();
#endif

#if defined(_LIBINTL_H) && defined(LIBINTL_VERSION)
	int wz_libintl_maj = LIBINTL_VERSION >> 16;
	int wz_libintl_min = (LIBINTL_VERSION >> 8) & 0xFF;
	int wz_libintl_rev = LIBINTL_VERSION & 0xFF;
	debug(LOG_WZ, "libintl version: %d.%d.%d", wz_libintl_maj, wz_libintl_min, wz_libintl_rev);
#endif

#ifdef WZ_OS_MAC
	{
		char resourcePath[PATH_MAX];
		CFURLRef resourceURL = CFBundleCopyResourcesDirectoryURL(CFBundleGetMainBundle());
		if (CFURLGetFileSystemRepresentation(resourceURL, true, (UInt8 *) resourcePath, PATH_MAX))
		{
			sstrcat(resourcePath, "/locale");
			boundAtLeastMainTextDomain = wzBindAllWZTextDomains(resourcePath);
		}
		else
		{
			debug(LOG_ERROR, "Could not change to resources directory.");
		}

		if (resourceURL != NULL)
		{
			CFRelease(resourceURL);
		}

		debug(LOG_INFO, "resourcePath is %s", resourcePath);
	}
#else
# ifdef WZ_LOCALEDIR
	// New locale-dir setup (CMake)
	#ifndef WZ_LOCALEDIR_ISABSOLUTE
	// Treat WZ_LOCALEDIR as a relative path - append to the install PREFIX
	const std::string prefixDir = getWZInstallPrefix();
	debug(LOG_WZ, "Install prefix: %s", prefixDir.c_str());
	const std::string dirSeparator(PHYSFS_getDirSeparator());
	std::string localeDir = prefixDir + dirSeparator + WZ_LOCALEDIR;
	boundAtLeastMainTextDomain = wzBindAllWZTextDomains(localeDir.c_str());
	#else
	// Treat WZ_LOCALEDIR as an absolute path, and use directly
	boundAtLeastMainTextDomain = wzBindAllWZTextDomains(WZ_LOCALEDIR);
	#endif
# else
	// Old locale-dir setup (autotools)
	boundAtLeastMainTextDomain = wzBindAllWZTextDomains(LOCALEDIR);
# endif
#endif // ifdef WZ_OS_MAC
	if (!boundAtLeastMainTextDomain)
	{
		debug(LOG_ERROR, "initI18n: bindtextdomain failed!");
	}

	// Set default text domain
	(void)textdomain(PACKAGE);
	debug(LOG_WZ, "textdomain: %s", PACKAGE);

	// Should come *after* bindTextDomain
	canUseLANGUAGEEnvVar = checkSupportsLANGUAGEenvVarOverride();

	if (!setLanguage(defaultLanguage.c_str())) // set to system default
	{
		// no system default?
		debug(LOG_ERROR, "initI18n: No system language found");
	}
}

// convert macro __DATE__ to ISO 8601 format
const char *getCompileDate()
{
	if (compileDate.empty())
	{
		std::istringstream date(__DATE__);
		std::string monthName;
		int day = 0, month = 0, year = 0;
		date >> monthName >> day >> year;

		std::string monthNames[] = {
						"Jan", "Feb", "Mar", "Apr",
						"May", "Jun", "Jul", "Aug",
						"Sep", "Oct", "Nov", "Dec"
		                           };
		for (int i = 0; i < 12; i++)
		{
			if (monthNames[i] == monthName)
			{
				month = i + 1;
				break;
			}
		}
		compileDate = astringf("%04d-%02d-%02d", year, month, day);
	}
	return compileDate.c_str();
}
