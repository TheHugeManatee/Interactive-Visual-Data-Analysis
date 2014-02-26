#pragma once
#include <map>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>

class SettingsStorage
{
public:
	SettingsStorage(void) {};
	~SettingsStorage(void) {};

	void Save(std::string filename) {
		std::ofstream f;
		f.open(filename);
		if(!f.is_open()) {
			std::cerr << "Error opening File!" << std::endl;
			return;
		}

		for(auto it=m_items.begin(); it != m_items.end(); ++it) {
			f << (*it).first << "=" << (*it).second << std::endl;
		}
		f.close();
	};

	bool Load(std::string filename) {
		m_items.clear();
		std::ifstream f;
		f.open(filename);
		if(!f.is_open()) {
			std::cerr << "Error opening File!" << std::endl;
			return false;
		}

		while(!f.eof()) {
			std::string line = readline(f);
			
			//skip blank lines and comments
			if(!line.size() || line[0]=='#') 
				continue;

			size_t p = line.find_first_of('=');
			if(p < 0)
				continue;

			std::string name = line.substr(0, p);
			std::string value = line.substr(p+1);
			std::cout << name << " = " << value << std::endl;
			m_items[name] = value;
		}
		f.close();
		return true;
	};

	void Print() 
	{
		for(auto it=m_items.begin(); it != m_items.end(); ++it) {
			std::cout << (*it).first << " = " << (*it).second << std::endl;
		}
	}
	void Reset()
	{
		m_items.clear();
	}

	void StoreString(std::string name, std::string value) {
		// TODO: parse newlines and replace them
		m_items[name] = value;
	};
	void GetString(std::string name, std::string &value) {
		if(m_items.find(name) == m_items.end()) return;
		value = m_items[name];
	};

	void StoreFloat(std::string name, float value) {
		std::stringstream ss;
		ss << value;
		m_items[name] = ss.str();
	}
	void GetFloat(std::string name, float &value) {
		if(m_items.find(name) == m_items.end()) return;
		std::stringstream ss(m_items[name]);
		ss >> value;
		if(ss.fail())
			value = 0.f;
	};

	void StoreFloat3(std::string name, float *value) {
		std::stringstream ss;
		ss << value[0] << " " << value[1] << " " << value[2];
		m_items[name] = ss.str();
	};
	void GetFloat3(std::string name, float* value) {
		if(m_items.find(name) == m_items.end()) return;
		std::stringstream ss(m_items[name]);
		ss >> value[0] >> value[1] >> value[2];
		if(ss.fail())
			value[0] = value[1] = value[2] = 0;
	};

	void StoreFloat4(std::string name, float *value) {
		std::stringstream ss;
		ss << value[0] << " " << value[1] << " " << value[2] << " " << value[3];
		m_items[name] = ss.str();
	};
	void GetFloat4(std::string name, float* value) {
		if(m_items.find(name) == m_items.end()) return;
		std::stringstream ss(m_items[name]);
		ss >> value[0] >> value[1] >> value[2] >> value[3];
		if(ss.fail())
			value[0] = value[1] = value[2] = value[3] = 0;
	};

	void StoreFloat4x4(std::string name, float *value) {
		std::stringstream ss;
		ss << value[0] << " " << value[1] << " " << value[2] << " " << value[3] << " ";
		ss << value[4] << " " << value[5] << " " << value[6] << " " << value[7] << " ";
		ss << value[8] << " " << value[9] << " " << value[10] << " " << value[11] << " ";
		ss << value[12] << " " << value[13] << " " << value[14] << " " << value[15];
		m_items[name] = ss.str();
	}
	void GetFloat4x4(std::string name, float *v) {
		if(m_items.find(name) == m_items.end()) return;
		std::stringstream ss(m_items[name]);
		ss >> v[0] >> v[1] >> v[2] >> v[3];
		ss >> v[4] >> v[5] >> v[6] >> v[7];
		ss >> v[8] >> v[9] >> v[10] >> v[11];
		ss >> v[12] >> v[13] >> v[14] >> v[15];
	};;

	void StoreInt(std::string name, int value) {
		std::stringstream ss;
		ss << value;
		m_items[name] = ss.str();
	};
	void GetInt(std::string name, int &value) {
		if(m_items.find(name) == m_items.end()) return;
		std::stringstream ss(m_items[name]);
		ss >> value;
		if(ss.fail())
			value = 0;
	};

	void StoreInt3(std::string name, int *value) {
		std::stringstream ss;
		ss << value[0] << " " << value[1] << " " << value[2];
		m_items[name] = ss.str();
	};
	void GetInt3(std::string name, int *value)
	{
		if(m_items.find(name) == m_items.end()) return;
		std::stringstream ss(m_items[name]);
		ss >> value[0] >> value[1] >> value[2];
		if(ss.fail())
			value[0] = value[1] = value[2] = value[3] = 0;
	}

	void StoreBool(std::string name, bool value) {
		m_items[name] = value?"1":"0";
	}
	void GetBool(std::string name, bool &value) {
		if(m_items.find(name) == m_items.end()) return;
		value = (m_items[name].length() && m_items[name][0] == '1');
	}

protected:
	std::map<std::string, std::string> m_items;

	std::string readline(std::ifstream &file) {
		//TODO: if we have lines that are longer than 4096, this will not work...
		char buf[4096];
		file.getline(buf, sizeof(buf));
		return std::string(buf);
	};
};

