#include "key_manager.hpp"
#include <mcp/common/common.hpp>

#include <cryptopp/sha.h>
#include <cryptopp/hkdf.h>

void mcp::kdf::phs(dev::Secret & result_a, std::string const & password_a, dev::h128 const & salt_a)
{
	std::lock_guard<std::mutex> lock(mutex);
	CryptoPP::HKDF<CryptoPP::SHA256> hkdf;
	hkdf.DeriveKey((Byte*) result_a.data(), result_a.size, (Byte*) password_a.data(), password_a.length(), salt_a.data(), salt_a.size, NULL, 0);
}

mcp::key_manager::key_manager(boost::filesystem::path const & application_path_a, mcp::key_store& store_a):
	m_backup_path(application_path_a / "backup"),
	m_store(store_a)
{
	auto transaction = m_store.create_transaction();
	auto it = m_store.keys_begin(transaction);
	for (; it.valid(); ++it)
	{
		dev::Address account(mcp::slice_to_account(it.key()));
		mcp::key_content key_content(it.value());
		m_key_contents[account] = key_content;
	}

	boost::filesystem::create_directories(m_backup_path);
}

bool mcp::key_manager::exists(dev::Address const & account_a)
{
	std::lock_guard<std::mutex> lock(m_key_contents_mutex);
	return m_key_contents.count(account_a) > 0;
}

bool mcp::key_manager::find(dev::Address const & account_a, mcp::key_content & kc_a)
{
	bool exists(true);
	std::lock_guard<std::mutex> lock(m_key_contents_mutex);
	if (m_key_contents.count(account_a))
	{
		kc_a = m_key_contents[account_a];
	}
	else
	{
		exists = false;
	}
	return exists;
}

std::list<dev::Address> mcp::key_manager::list()
{
	std::list<dev::Address> accounts;
	std::lock_guard<std::mutex> lock(m_key_contents_mutex);
	for (auto pair : m_key_contents)
		accounts.push_back(pair.first);
	return accounts;
}

dev::Address mcp::key_manager::create(std::string const & password_a, bool generate_work_a, bool const & is_backup_a)
{
	dev::Secret prv;
	random_pool.GenerateBlock((Byte*)prv.data(), prv.size);

	mcp::key_content kc(gen_key_content(prv, password_a));
	add_or_update_key(kc, is_backup_a);

	return kc.account;
}

bool mcp::key_manager::change_password(dev::Address const & account_a, std::string const & old_password_a, std::string const & new_password_a)
{
	dev::Secret prv;
	bool error(decrypt_prv(account_a, old_password_a, prv));
	if (!error)
	{
		mcp::key_content kc(gen_key_content(prv, new_password_a));
		add_or_update_key(kc);
	}
	return error;
}

bool mcp::key_manager::remove(dev::Address const & account_a, std::string const & password_a)
{
	dev::Secret prv;
	bool error(decrypt_prv(account_a, password_a, prv));
	if (!error)
	{
		{
			std::lock_guard<std::mutex> lock(m_unlocked_mutex);
			m_unlocked.erase(account_a);
		}
		{
			std::lock_guard<std::mutex> lock(m_key_contents_mutex);
			m_key_contents.erase(account_a);
		}
		mcp::db::db_transaction transaction = m_store.create_transaction();
		m_store.keys_del(transaction, account_a);
	}

	return error;
}

bool mcp::key_manager::import(std::string const & json_a, key_content & kc_a, bool generate_work_a)
{
	bool error(false);
	mcp::key_content kc(error, json_a);
	if (!error)
	{
		add_or_update_key(kc);
		kc_a = kc;
	}
	return error;
}

mcp::key_content mcp::key_manager::importRawKey(dev::Secret & prv, std::string const & password)
{
	mcp::key_content kc = gen_key_content(prv, password);
	add_or_update_key(kc, true);
	return kc;
}

bool mcp::key_manager::decrypt_prv(dev::Address const & account, std::string const & password_a, dev::Secret & prv)
{
	bool error(false);
	mcp::key_content kc;
	{
		std::lock_guard<std::mutex> lock(m_key_contents_mutex);
		if (m_key_contents.count(account))
			kc = m_key_contents[account];
		else
			error = true;
	}

	if (!error)
		error = decrypt_prv(kc, password_a, prv);

	return error;
}

bool mcp::key_manager::decrypt_prv(mcp::key_content const & kc_a, std::string const & password_a, dev::Secret & prv)
{
	dev::Secret derive_pwd;
	m_kdf.phs(derive_pwd, password_a, kc_a.kdf_salt);

	mcp::encry::dencryption(
		(unsigned char*)prv.data(),
		(const unsigned char*)kc_a.ciphertext.data(),
		kc_a.ciphertext.size,
		(const unsigned char*)kc_a.iv.data(),
		(const unsigned char*)derive_pwd.data()
	);

	return kc_a.account != dev::toAddress(dev::toPublic(prv));
}

bool mcp::key_manager::is_locked(dev::Address const & account_a)
{
	std::lock_guard<std::mutex> lock(m_unlocked_mutex);
	return m_unlocked.count(account_a) == 0;
}

bool mcp::key_manager::find_unlocked_prv(dev::Address const & account_a, dev::Secret & prv)
{
	bool exists(true);
	std::lock_guard<std::mutex> lock(m_unlocked_mutex);
	if (m_unlocked.count(account_a))
		prv = m_unlocked[account_a];
	else
		exists = false;
	return exists;
}

void mcp::key_manager::lock(dev::Address const & account_a)
{
	std::lock_guard<std::mutex> lock(m_unlocked_mutex);
	m_unlocked.erase(account_a);
}

bool mcp::key_manager::unlock(dev::Address const & account_a, std::string const & password_a)
{
	dev::Secret prv;
	bool error(decrypt_prv(account_a, password_a, prv));
	if (!error)
	{
		std::lock_guard<std::mutex> lock(m_unlocked_mutex);
		m_unlocked[account_a] = prv;
	}
	return error;
}

void mcp::key_manager::write_backup(dev::Address const & account, std::string const & json)
{
	std::ofstream backup_file;
	std::string file_name((m_backup_path / (account.hexPrefixed() + ".json")).string());
	backup_file.open(file_name);
	if (!backup_file.fail())
	{
		backup_file << json;
	}
}

mcp::key_content mcp::key_manager::gen_key_content(dev::Secret const & prv, std::string const & password_a)
{
	dev::h128 kdf_salt;
	random_pool.GenerateBlock(kdf_salt.data(), kdf_salt.size);

	dev::Secret derive_pwd;
	m_kdf.phs(derive_pwd, password_a, kdf_salt);

	dev::h128 iv;
	random_pool.GenerateBlock(iv.data(), iv.size);

	dev::h256 ciphertext;
	mcp::encry::encryption(
		(unsigned char*)ciphertext.data(),
		(const unsigned char*)prv.data(),
		prv.size,
		(const unsigned char*)iv.data(),
		(const unsigned char*)derive_pwd.data()
	);

	return mcp::key_content(dev::toAddress(dev::toPublic(prv)), kdf_salt, iv, ciphertext);
}

void mcp::key_manager::add_or_update_key(mcp::key_content const & kc, bool const & is_backup_a)
{
	{
		std::lock_guard<std::mutex> lock(m_key_contents_mutex);
		m_key_contents[kc.account] = kc;
	}

	mcp::db::db_transaction transaction = m_store.create_transaction();
	m_store.keys_put(transaction, kc.account, kc);

	if (is_backup_a)
		write_backup(kc.account, kc.to_json());
}

std::pair<bool, Secret> mcp::key_manager::authenticate(dev::Address account, boost::optional<std::string> const & password)
{
	std::pair<bool, Secret> ret;
	
	if (m_key_contents.count(account))
	{
		if (m_unlocked.count(account))
		{
			ret = std::make_pair(false, m_unlocked[account]);
		}
		else if (password)
		{
			dev::Secret prv;
			if (decrypt_prv(account, *password, prv))
				BOOST_THROW_EXCEPTION(AccountLocked());
			ret = std::make_pair(false, prv);
		}
		else
			BOOST_THROW_EXCEPTION(AccountLocked());
	}
	else
		BOOST_THROW_EXCEPTION(UnknownAccount());
	
	return ret;
}