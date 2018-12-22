#include "pch.h"
#include "program.h"
#include <windows.h>
#include <math.h>


template <class T>
T* CProgram<T>::head = nullptr;

template <class T>
T* CProgram<T>::tail = nullptr;

template <class T>
CProgram<T>::CProgram()
{
	next = nullptr;
	prev = nullptr;
}

template <class T>
CProgram<T>::~CProgram()
{
}

template <class T>
void CProgram<T>::make_new_head()
{
	try {
		head = new T;
	}
	catch (const std::bad_alloc& ba) {
		std::cout << ba.what() << ": " << __FUNCTION__ << std::endl;
	}
	if (head) tail = head;
}

template <class T>
void CProgram<T>::make_new_tail(T* &new_tail)
{
	try {
		new_tail = new T;
	}
	catch (const std::bad_alloc& ba) {
		std::cout << ba.what() << ": " << __FUNCTION__ << std::endl;
	}
	if (new_tail) {
		new_tail->prev = tail;
		tail = new_tail;
	}
}

template <class T>
void CProgram<T>::insert()
{
	if (!head)
		make_new_head();
	else
		make_new_tail(tail->next);
}

template <class T>
std::string CProgram<T>::cmd_value(std::string str, std::string::size_type pos)
{
	std::string::size_type i;

	i = pos;
	pos++;
	while (isalpha(str[pos]) == 0 && pos < str.size())
		pos++;

	return str.substr(i, pos - i);
}

double CPlan::offset_x = 0;

CPlan::CPlan()
{
	addr = -1;
	r = 0;
	x = 0;
	y = 0;
	part = nullptr;
}

CPlan::CPlan(const char* program) : addr{ -1 }, r{ 0 }, x{ 0 }, y{ 0 }, part{nullptr}
{
	plan_parser(program);
}

CPlan::~CPlan()
{
}

void CPlan::cleanup()
{
	CPlan* node = head;

	if (node)
		node->part->cleanup();

	while (node) {
		head = node->next;
		delete(node);
		node = head;
	}
}

void CPlan::job_code(std::string str)
{
	std::string::size_type pos;
	std::string buf;

	insert();

	if (tail) {
		if (tail->prev) {
			tail->addr = tail->prev->addr;
			tail->x = tail->prev->x;
			tail->y = tail->prev->y;
			tail->r = tail->prev->r;
		}
	}

	pos = 0;
	while (pos < str.size()) {
		if (str[pos] == 'G') {
			buf = cmd_value(str, pos);
			if (tail)
				tail->addr = atoi((buf.substr(1, buf.size() - 1)).c_str());
			pos = pos + buf.size() - 1;
		}
		else if (str[pos] == 'L') {
			buf = cmd_value(str, pos);
			if (tail)
				tail->r = atoi((buf.substr(1, buf.size() - 1)).c_str());
			pos = pos + buf.size() - 1;
		}
		else if (str[pos] == 'X') {
			buf = cmd_value(str, pos);
			if (tail)
				tail->x = atof((buf.substr(1, buf.size() - 1)).c_str()) - offset_x;
			pos = pos + buf.size() - 1;
		}
		else if (str[pos] == 'Y') {
			buf = cmd_value(str, pos);
			if (tail)
				tail->y = atof((buf.substr(1, buf.size() - 1)).c_str());
			pos = pos + buf.size() - 1;
		}
		pos++;
	}
}

void CPlan::plan_parser(const char* program)
{
	FILE *ifs = nullptr;
	if ((fopen_s(&ifs, program, "r")) != 0) {
		std::cout << "Can't open file: " << program << std::endl;
		return;
	}

	char lbuf[10000];
	std::string this_round;
	std::string::size_type pos;

	bool jobcode = false;
	bool partcode = false;

	offset_x = 0;

	CPart p;

	for (; ; ) {
		lbuf[0] = 0;
		if (ifs != nullptr) {
			if (fgets(lbuf, sizeof(lbuf), ifs) == nullptr) {
				break;
			}
		}
		this_round = lbuf;

		if (this_round.size() <= 0)
			continue;

		pos = this_round.find("JobCode");
		if (pos != std::string::npos) {
			jobcode = true;
			continue;
		}

		if (jobcode) {
			if ((pos = this_round.find("G29")) != std::string::npos)
				continue;
			if ((pos = this_round.find("G4M19")) != std::string::npos) {
				pos = this_round.find("=", pos + 1);
				if (pos >= this_round.size())
					continue;
				this_round = this_round.substr(pos + 1);
				if (this_round[0] == ' ') {
					pos = 0;
					pos = this_round.find(')', pos + 1);
					if (pos >= this_round.size())
						continue;
					this_round = this_round.substr(1, pos - 1);
					offset_x = atof(this_round.c_str());
				}
				continue;
			}
			if ((pos = this_round.find("G99")) != std::string::npos) {
				jobcode = false;
				continue;
			}
			job_code(this_round);
		}

		pos = this_round.find("PartCode");
		if (pos != std::string::npos) {
			pos = this_round.find('=', pos + 1);
			if (pos >= this_round.size())
				continue;
			this_round = this_round.substr(pos + 1);
			if (this_round[0] == '"') {
				pos = 0;
				pos = this_round.find('"', pos + 1);
				if (pos >= this_round.size())
					continue;
				this_round = this_round.substr(1, pos - 1);
			}
			CPart part(this_round);
			partcode = true;
			continue;
		}

		if (partcode) {
			if ((pos = this_round.find("<Contour>")) != std::string::npos)
				continue;
			if ((pos = this_round.find("</Contour>")) != std::string::npos)
				continue;
			if ((pos = this_round.find("G98")) != std::string::npos) {
				partcode = false;
				continue;
			}
			p.insert_part(this_round);
		}
	}

	if (ifs)
		fclose(ifs);

	p.filter();
	part_list();
	calculation();
	cleanup();
}

double CPlan::arc_length(CData *data, int direction)
{
	double x1 = 0;
	double y1 = 0;
	double x2 = 0;
	double y2 = 0;
	double i = 0;
	double j = 0;
	double r = 0;
	double a = 0;
	double a1 = 0;
	double a2 = 0;
	double l = 0;
	double v, v1, v2;
	int h[2] = { 0, 0 };
	double pi = 3.1415926535897932384626433832795;

	if (data) {
		x2 = data->x;
		y2 = data->y;
		if (data->next) {
			x1 = data->next->x;
			y1 = data->next->y;
		}
		if (data->trail) {
			i = data->trail->i;
			j = data->trail->j;
		}
		else {
			char buffer[100];
			_snprintf_s(buffer, _countof(buffer), _TRUNCATE, "The coordinates of the center of the circle are missing! [X=%.2f Y=%.2f I=? J=?]", x2, y2);
			std::cout << "Warning: " << buffer << std::endl;
		}
	}

	if (y1 < j)
		h[0] = 2;
	else
		h[0] = 1;
	if (y2 < j)
		h[1] = 2;
	else
		h[1] = 1;

	r = sqrt(pow(i - x1, 2) + pow(j - y1, 2));
	if (r <= 0) return 0;
	v = sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2)) / (2 * r);
	if (fabs(v) > (double)1.0)
		v = 1.0;
	a = asin(v) * 360 / pi;
	v1 = sqrt(pow(x1 - (i + r), 2) + pow(y1 - j, 2)) / (2 * r);
	if (fabs(v1) > (double)1.0)
		v1 = 1.0;
	a1 = asin(v1) * 360 / pi;
	v2 = sqrt(pow(x2 - (i + r), 2) + pow(y2 - j, 2)) / (2 * r);
	if (fabs(v2) > (double)1.0)
		v2 = 1.0;
	a2 = asin(v2) * 360 / pi;

	if (h[0] == 2)
		a1 = 360 - a1;
	if (h[1] == 2)
		a2 = 360 - a2;

	//	x2 = i + cos((a2 - a1) * (pi / 180)) * r;
	y2 = j + sin((a2 - a1) * (pi / 180)) * r;

	l = (a * pi * r) / 180;

	if ((y2 < j && direction == 3) || (y2 > j && direction == 2))
		l = 2 * pi * r - l;

	return l;
}

void CPlan::calculation()
{
	double tc = 0;
	double tot_tc = 0;
	double tc_cw_str = 0;		// straight cutting
	double tc_cw_arc = 0;		// arc cutting
	double tc_pulse_str = 0;	// straight pulse
	double tc_pulse_arc = 0;	// arc pulse
	double tc_macro1_str = 0;	// straight processmacro 1
	double tc_macro1_arc = 0;	// arc processmacro 1
	double tc_macro2_str = 0;	// straight processmacro 2
	double tc_macro2_arc = 0;	// arc processmacro 2
	double tc_macro3_str = 0;	// straight processmacro 3
	double tc_macro3_arc = 0;	// arc processmacro 3
	double tc_macro4_str = 0;	// straight processmacro 4
	double tc_macro4_arc = 0;	// arc processmacro 4
	double tc_macro5_str = 0;	// straight processmacro 5
	double tc_macro5_arc = 0;	// arc processmacro 5
	double rt = 0;				// rapidtraverse
	double tm = 0;				// traversemark
	double x = 0;
	double y = 0;
	int pierces = 0;
	int m04 = 0;
	int m14 = 0;
	int m18 = 0;

	std::ostringstream q;
	std::ostringstream _tc_cw_str, _tc_cw_arc, _tc_pulse_str, _tc_pulse_arc, _tc_macro1_str, _tc_macro1_arc, _tc_macro2_str, _tc_macro2_arc;
	std::ostringstream _tc_macro3_str, _tc_macro3_arc, _tc_macro4_str, _tc_macro4_arc, _tc_macro5_str, _tc_macro5_arc;
	std::ostringstream _tc, _tm, _rt, _pierce, _m04, _m14, _m18;

	cmap::const_iterator it;

	CPlan* plan = head;
	CData* data = nullptr;

	while (plan) {
		if (plan->part)
			data = plan->part->data;

		while (data) {
			if (!data->next)
				break;
			data = data->next;
		}

		while (data) {
			switch (data->addr)
			{
			case 0:
				rt = rt + sqrt(pow((data->x + plan->x) - x, 2) + pow((data->y + plan->y) - y, 2));
				break;
			case 1:
				// data->op:	0 - cutting, 1 - pulse, 2 - engraving, 3 - processmacro 1, 4 - processmacro 2
				//				5 - processmacro 3, 6 - processmacro 4, 7 - processmacro 5
				if (data->op == 0)
					tc_cw_str = tc_cw_str + sqrt(pow((data->x + plan->x) - x, 2) + pow((data->y + plan->y) - y, 2));
				else if (data->op == 1)
					tc_pulse_str = tc_pulse_str + sqrt(pow((data->x + plan->x) - x, 2) + pow((data->y + plan->y) - y, 2));
				else if (data->op == 2)
					tm = tm + sqrt(pow((data->x + plan->x) - x, 2) + pow((data->y + plan->y) - y, 2));
				else if (data->op == 3)
					tc_macro1_str = tc_macro1_str + sqrt(pow((data->x + plan->x) - x, 2) + pow((data->y + plan->y) - y, 2));
				else if (data->op == 4)
					tc_macro2_str = tc_macro2_str + sqrt(pow((data->x + plan->x) - x, 2) + pow((data->y + plan->y) - y, 2));
				else if (data->op == 5)
					tc_macro3_str = tc_macro3_str + sqrt(pow((data->x + plan->x) - x, 2) + pow((data->y + plan->y) - y, 2));
				else if (data->op == 6)
					tc_macro4_str = tc_macro4_str + sqrt(pow((data->x + plan->x) - x, 2) + pow((data->y + plan->y) - y, 2));
				else if (data->op == 7)
					tc_macro5_str = tc_macro5_str + sqrt(pow((data->x + plan->x) - x, 2) + pow((data->y + plan->y) - y, 2));
				break;
			case 2:
				if (data->op == 0)
					tc_cw_arc = tc_cw_arc + arc_length(data, 2);
				else if (data->op == 1)
					tc_pulse_arc = tc_pulse_arc + arc_length(data, 2);
				else if (data->op == 2)
					tm = tm + arc_length(data, 2);
				else if (data->op == 3)
					tc_macro1_arc = tc_macro1_arc + arc_length(data, 2);
				else if (data->op == 4)
					tc_macro2_arc = tc_macro2_arc + arc_length(data, 2);
				else if (data->op == 5)
					tc_macro3_arc = tc_macro3_arc + arc_length(data, 2);
				else if (data->op == 6)
					tc_macro4_arc = tc_macro4_arc + arc_length(data, 2);
				else if (data->op == 7)
					tc_macro5_arc = tc_macro5_arc + arc_length(data, 2);
				break;
			case 3:
				if (data->op == 0)
					tc_cw_arc = tc_cw_arc + arc_length(data, 3);
				else if (data->op == 1)
					tc_pulse_arc = tc_pulse_arc + arc_length(data, 3);
				else if (data->op == 2)
					tm = tm + arc_length(data, 3);
				else if (data->op == 3)
					tc_macro1_arc = tc_macro1_arc + arc_length(data, 3);
				else if (data->op == 4)
					tc_macro2_arc = tc_macro2_arc + arc_length(data, 3);
				else if (data->op == 5)
					tc_macro3_arc = tc_macro3_arc + arc_length(data, 3);
				else if (data->op == 6)
					tc_macro4_arc = tc_macro4_arc + arc_length(data, 3);
				else if (data->op == 7)
					tc_macro5_arc = tc_macro5_arc + arc_length(data, 3);
				break;
			}

			x = data->x + plan->x;
			y = data->y + plan->y;

			data = data->prev;
		}

		if (plan->part) {
			m04 = m04 + plan->part->m04;
			m14 = m14 + plan->part->m14;
			m18 = m18 + plan->part->m18;

			pierces = plan->part->m04 + plan->part->m14 + plan->part->m18;
			it = pierces_map_.find(plan->part->code);
			if (it != pierces_map_.end())
				pierces = pierces + atoi(it->second.c_str());
			q.str("");
			q << pierces;
			pierces_map_[plan->part->code] = q.str();

			tc = tc_cw_str + tc_cw_arc + tc_pulse_str + tc_pulse_arc + tc_macro1_str + tc_macro1_arc + tc_macro2_str + tc_macro2_arc + tc_macro3_str + tc_macro3_arc + tc_macro4_str + tc_macro4_arc + tc_macro5_str + tc_macro5_arc - tot_tc;
			tot_tc += tc;
			it = cutlength_map_.find(plan->part->code);
			if (it != cutlength_map_.end())
				tc = tc + (atof(it->second.c_str()));
			q.str("");
			q << tc;
			cutlength_map_[plan->part->code] = q.str();
		}

		plan = plan->next;
	}

	update_pierces();
	update_cutlength();

	_tc << tc_cw_str + tc_cw_arc + tc_pulse_str + tc_pulse_arc + tc_macro1_str + tc_macro1_arc + tc_macro2_str + tc_macro2_arc + tc_macro3_str + tc_macro3_arc + tc_macro4_str + tc_macro4_arc + tc_macro5_str + tc_macro5_arc;
	_tc_cw_str << tc_cw_str;
	_tc_cw_arc << tc_cw_arc;
	_tc_pulse_str << tc_pulse_str;
	_tc_pulse_arc << tc_pulse_arc;
	_tc_macro1_str << tc_macro1_str;
	_tc_macro1_arc << tc_macro1_arc;
	_tc_macro2_str << tc_macro2_str;
	_tc_macro2_arc << tc_macro2_arc;
	_tc_macro3_str << tc_macro3_str;
	_tc_macro3_arc << tc_macro3_arc;
	_tc_macro4_str << tc_macro4_str;
	_tc_macro4_arc << tc_macro4_arc;
	_tc_macro5_str << tc_macro5_str;
	_tc_macro5_arc << tc_macro5_arc;
	_tm << tm;
	_rt << rt;
	_pierce << m04 + m14 + m18;
	_m04 << m04;
	_m14 << m14;
	_m18 << m18;

	calc_map_["tc"] = _tc.str();
	calc_map_["tc_cw_str"] = _tc_cw_str.str();
	calc_map_["tc_cw_arc"] = _tc_cw_arc.str();
	calc_map_["tc_pulse_str"] = _tc_pulse_str.str();
	calc_map_["tc_pulse_arc"] = _tc_pulse_arc.str();
	calc_map_["tc_macro1_str"] = _tc_macro1_str.str();
	calc_map_["tc_macro1_arc"] = _tc_macro1_arc.str();
	calc_map_["tc_macro2_str"] = _tc_macro2_str.str();
	calc_map_["tc_macro2_arc"] = _tc_macro2_arc.str();
	calc_map_["tc_macro3_str"] = _tc_macro3_str.str();
	calc_map_["tc_macro3_arc"] = _tc_macro3_arc.str();
	calc_map_["tc_macro4_str"] = _tc_macro4_str.str();
	calc_map_["tc_macro4_arc"] = _tc_macro4_arc.str();
	calc_map_["tc_macro5_str"] = _tc_macro5_str.str();
	calc_map_["tc_macro5_arc"] = _tc_macro5_arc.str();
	calc_map_["tm"] = _tm.str();
	calc_map_["rt"] = _rt.str();
	calc_map_["pierce"] = _pierce.str();
	calc_map_["m04"] = _m04.str();
	calc_map_["m14"] = _m14.str();
	calc_map_["m18"] = _m18.str();
}

void CPlan::update_cutlength()
{
	cmap::const_iterator it;
	double tc;
	int n;
	std::ostringstream q;

	for (it = cutlength_map_.begin(); it != cutlength_map_.end(); it++)
	{
		tc = atof(it->second.c_str());
		n = atoi(get_part_quantity(it->first).c_str());

		if (n != 0)
			tc = tc / n;
		else
			tc = 0;

		q.str("");
		q << tc;
		cutlength_map_[it->first] = q.str();
	}
}

void CPlan::update_pierces()
{
	cmap::const_iterator it;
	int pierces;
	int n;
	std::ostringstream q;

	for (it = pierces_map_.begin(); it != pierces_map_.end(); it++)
	{
		pierces = atoi(it->second.c_str());
		n = atoi(get_part_quantity(it->first).c_str());

		if (n != 0)
			pierces = pierces / n;
		else
			pierces = 0;

		q.str("");
		q << pierces;
		pierces_map_[it->first] = q.str();
	}
}

std::string CPlan::get_part_quantity(const std::string& part, const std::string& default_value) const
{
	cmap::const_iterator it = part_map_.find(part);

	if (it != part_map_.end())
		return it->second;

	return default_value;
}

std::string CPlan::get_calc(const std::string& calc, const std::string& default_value) const
{
	cmap::const_iterator it = calc_map_.find(calc);

	if (it != calc_map_.end())
		return it->second;

	return default_value;
}

std::string CPlan::get_cutlength(const std::string& part, const std::string& default_value) const
{
	cmap::const_iterator it = cutlength_map_.find(part);

	if (it != cutlength_map_.end())
		return it->second;

	return default_value;
}

std::string CPlan::get_pierces(const std::string& part, const std::string& default_value) const
{
	cmap::const_iterator it = pierces_map_.find(part);

	if (it != pierces_map_.end())
		return it->second;

	return default_value;
}

int CPlan::part_counter(int n, CPart* part)
{
	CPlan* node = head;
	CPlan* tmp = node;
	int i = 0;

	while (node) {
		tmp = tmp->next;
		if (node->r == n && node->addr == 52) {
			i++;
			if (part)
				node->part = part;
		}
		node = tmp;
	}

	return i;
}

void CPlan::part_list()
{
	if (!head)
		return;

	CPlan *plan = head;
	CPart* node = plan->part;
	CPart* tmp = nullptr;
	int i;
	int p;
	std::ostringstream q;
	cmap::const_iterator it;

	while (!node && plan) {
		plan = plan->next;
		if (plan)
			node = plan->part;
	}
	while (node) {
		i = 0;
		p = 0;
		it = part_map_.find(node->code);
		if (it == part_map_.end()) {
			if (node->n > 0) {
				i = node->n;
				p++;
			}
			tmp = node->next;
			while (tmp) {
				if (node->code == tmp->code) {
					if (tmp->n > 0) {
						i += tmp->n;
						p++;
					}
				}
				tmp = tmp->next;
			}
			if (p > 0) {
				((p % 2) == 0) ? (i = i - (p / 2)) : (i = i - ((p - 1) / 2));
				q.str("");
				q << i;
				part_map_[node->code] = q.str();
			}
		}
		node = node->next;
	}
}

CPart::CPart()
{
	n = 0;
	m04 = 0;
	m14 = 0;
	m18 = 0;
	data = nullptr;
}

CPart::CPart(std::string str) : n{ 0 }, m04{ 0 }, m14{ 0 }, m18{ 0 }, data{nullptr}
{
	insert();
	if (tail)
		tail->code = str;
}

CPart::~CPart()
{
}

void CPart::insert_object(CData* pt)
{
	if (tail && pt) {
		pt->next = tail->data;
		if (tail->data)
			tail->data->prev = pt;
		tail->data = pt;
	}
}

void CPart::cmd_inherit(CData* data)
{
	if (data && data->next) {
		data->addr = data->next->addr;
		if (data->addr == 2 || data->addr == 3) {
			try {
				data->trail = new CTrailer();
			}
			catch (const std::bad_alloc& ba) {
				std::cout << ba.what() << ": " << __FUNCTION__ << std::endl;
			}
			if (data->trail && data->next->trail) {
				data->trail->i = data->next->trail->i;
				data->trail->j = data->next->trail->j;
			}
		}
		data->x = data->next->x;
		data->y = data->next->y;
		data->op = data->next->op;
	}
}

void CPart::insert_part(std::string str)
{
	std::string::size_type pos;
	std::string buf;
	CPlan plan;
	CData* tmp = nullptr;
	int pierce;

	try {
		data = new CData();
	}
	catch (const std::bad_alloc& ba) {
		std::cout << ba.what() << ": " << __FUNCTION__ << std::endl;
	}
	insert_object(data);
	cmd_inherit(data);

	pos = 0;
	while (pos < str.size()) {
		if (str[pos] == 'G') {
			buf = cmd_value(str, pos);
			if (data) {
				data->addr = atoi((buf.substr(1, buf.size() - 1)).c_str());
				if ((data->addr == 2 || data->addr == 3) && !(data->trail)) {
					try {
						data->trail = new CTrailer();
					}
					catch (const std::bad_alloc& ba) {
						std::cout << ba.what() << ": " << __FUNCTION__ << std::endl;
					}
					if (data->trail) {
						tmp = data->next;
						while (tmp) {
							if (tmp->trail) {
								data->trail->i = tmp->trail->i;
								data->trail->j = tmp->trail->j;
								break;
							}
							tmp = tmp->next;
						}
					}
				}
				else if ((data->addr != 2 && data->addr != 3) && (data->trail)) {
					delete(data->trail);
					data->trail = nullptr;
				}
			}
			pos = pos + buf.size() - 1;
		}
		else if (str[pos] == 'X') {
			buf = cmd_value(str, pos);
			if (data)
				data->x = atof((buf.substr(1, buf.size() - 1)).c_str());
			pos = pos + buf.size() - 1;
		}
		else if (str[pos] == 'Y') {
			buf = cmd_value(str, pos);
			if (data)
				data->y = atof((buf.substr(1, buf.size() - 1)).c_str());
			pos = pos + buf.size() - 1;
		}
		else if (str[pos] == 'I') {
			buf = cmd_value(str, pos);
			if (data->trail)
				data->trail->i = atof((buf.substr(1, buf.size() - 1)).c_str());
			pos = pos + buf.size() - 1;
		}
		else if (str[pos] == 'J') {
			buf = cmd_value(str, pos);
			if (data->trail)
				data->trail->j = atof((buf.substr(1, buf.size() - 1)).c_str());
			pos = pos + buf.size() - 1;
		}
		else if (str[pos] == 'L') {
			buf = cmd_value(str, pos);
			if (tail)
				tail->n = plan.part_counter(atoi((buf.substr(1, buf.size() - 1)).c_str()), tail);
		}
		else if (str[pos] == 'S') {
			buf = cmd_value(str, pos);
			if (data)
				data->op = atoi((buf.substr(1, buf.size() - 1)).c_str());
			pos = pos + buf.size() - 1;
		}
		else if (str[pos] == 'M') {
			buf = cmd_value(str, pos);
			pierce = atoi((buf.substr(1, buf.size() - 1)).c_str());
			if (tail) {
				if (data->op != 2) {
					if (pierce == 4)
						(tail->m04)++;
					else if (pierce == 14) {
						(tail->m14)++;
						(tail->m04)--;
					}
					else if (pierce == 18) {
						(tail->m18)++;
						(tail->m04)--;
					}
				}
			}
			pos = pos + buf.size() - 1;
		}

		pos++;
	}
}

CData* CPart::replace(CData* data)
{
	CData* tmp = data;

	if (tmp->prev)
		tmp->prev->next = tmp->next;
	if (tmp->next)
		tmp->next->prev = tmp->prev;

	if (tmp->trail)
		delete(tmp->trail);

	data = tmp->next;
	delete(tmp);

	return data;
}

void CPart::filter()
{
	CPart* node = head;
	CData* data = nullptr;
	bool fg = true;

	while (node) {
		data = node->data;
		while (data) {
			if (data->addr < 0 || data->addr > 3) {
				if (data == node->data) {
					data = data->next;
					node->data = data;
					if (data->prev->trail)
						delete(data->prev->trail);
					delete(data->prev);
					data->prev = nullptr;
					continue;
				}
				else {
					data = replace(data);
					continue;
				}
			}
			else if (data->addr != 0) {
				if (data->op == 2 && fg)
					fg = true;
				else
					fg = false;
			}
			data = data->next;
		}

		if (fg)
			node->n = 0;

		fg = true;
		node = node->next;
	}
	remove_surplus();
}

void CPart::remove_surplus()
{
	CPart* node = head;
	CData* data = nullptr;
	bool i;

	while (node) {
		data = node->data;
		while (data) {
			i = false;
			if (data->next) {
				if (data->addr == data->next->addr && data->x == data->next->x && data->y == data->next->y) {
					i = true;
					if (data->trail && data->next->trail) {
						i = false;
						if (data->trail->i == data->next->trail->i && data->trail->j == data->next->trail->j)
							i = true;
					}
				}
			}
			if (i == true) {
				data = data->next;
				data = replace(data);
				continue;
			}
			data = data->next;
		}
		node = node->next;
	}
}

void CPart::cleanup()
{
	CPart* node = head;

	while (node) {
		head = node->next;
		if (node->data)
			node->data->cleanup();
		delete(node);
		node = head;
	}
}

CData::CData()
{
	addr = -1;
	x = 0;
	y = 0;
	op = -1;
	next = nullptr;
	prev = nullptr;
	trail = nullptr;
}

CData::~CData()
{
}

void CData::cleanup()
{
	CData *node = this;
	CData *tmp = node;
	while (node) {
		if (node->trail)
			delete(node->trail);
		node = node->next;
		delete(tmp);
		tmp = node;
	}
}

CTrailer::CTrailer()
{
	i = 0;
	j = 0;
}

CTrailer::~CTrailer()
{
}
