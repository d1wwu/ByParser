#pragma once

#include <iostream>
#include <string>
#include <map>
#include <sstream>


typedef std::map<std::string, std::string> cmap;

class CData;
class CTrailer;
class CPart;

template <class T>
class CProgram
{
protected:
	static T* head;
	static T* tail;

	void make_new_head();
	void make_new_tail(T* &new_tail);
	void insert();
	std::string cmd_value(std::string str, std::string::size_type pos);

public:
	T* next;
	T* prev;

	CProgram();
	virtual ~CProgram();
};

class CPlan : public CProgram<CPlan>
{
	void job_code(std::string str);
	void update_cutlength();
	void update_pierces();
public:
	int addr;
	int r;
	double x;
	double y;
	CPart* part;

	cmap calc_map_;
	cmap part_map_;
	cmap cutlength_map_;
	cmap pierces_map_;

	static double offset_x;

	CPlan();
	CPlan(const char* program);
	virtual ~CPlan();

	void plan_parser(const char* program);
	int part_counter(int n, CPart* part = nullptr);
	void calculation();
	double arc_length(CData *data, int direction);
	void cleanup();
	void part_list();
	std::string get_calc(const std::string& calc, const std::string& default_value = std::string()) const;
	std::string get_part_quantity(const std::string& part, const std::string& default_value = std::string()) const;
	std::string get_cutlength(const std::string& part, const std::string& default_value = std::string()) const;
	std::string get_pierces(const std::string& part, const std::string& default_value = std::string()) const;
};

class CPart : public CProgram<CPart>
{
	void insert_object(CData *pt);
	void cmd_inherit(CData *data);
	CData* replace(CData *data);
	void remove_surplus();
public:
	int n;
	int m04;
	int m14;
	int m18;
	std::string code;
	CData *data;

	CPart();
	CPart(std::string str);
	virtual ~CPart();

	void insert_part(std::string str);
	void filter();
	void cleanup();
};

class CData : public CPart
{
public:
	int addr;
	double x;
	double y;
	int op;
	CData* next;
	CData* prev;
	CTrailer* trail;

	CData();
	virtual ~CData();

	virtual void cleanup();
};

class CTrailer
{
public:
	double i;
	double j;

	CTrailer();
	virtual ~CTrailer();
};
