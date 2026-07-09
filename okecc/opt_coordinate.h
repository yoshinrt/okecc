//////////////////////////////////////////////////////////////////////////////

void InitCoordinate(){
	m_cartesian_pos		= 0;
	m_height			= int2dist(800);
	m_width				= int2dist(800);
	m_x					= 0;
	m_y					= 0;
	
	m_span				= int2span(360);
	m_dist				= int2dist(800);
	m_dir				= 0;
}

int int2dist(int val){
	int sign = val < 0 ? -1 : 1;
	int v = std::abs(val);

	if(v <= 100) return (v / 2) * sign;
	if(v <= 200) return ((v - 100) / 5 + (100 / 2)) * sign;
	if(v <= 400) return ((v - 200) / 10 + (100 / 5) + (100 / 2)) * sign;
	if(v <= 800) return ((v - 400) / 20 + (200 / 10) + (100 / 5) + (100 / 2)) * sign;

	Error(std::format("Value {} is out of range [-800 - 800]", val));
	return 0;
}

int dist2int(int val){
	int sign = val < 0 ? -1 : 1;
	int v = std::abs(val);
	if(v <= 50) return v * 2 * sign;
	if(v <= 70) return (100 + (v - 50) * 5) * sign;
	if(v <= 90) return (200 + (v - 70) * 10) * sign;
	return (400 + (v - 90) * 20) * sign;
}

int int2angle(int val){
	if (val < -180 || 180 < val) Error(std::format("Value {} is out of range [-180 - 180]", val));
	if (val < 0) val += 360;
	return val / 2;
}

int angle2int(int val){
	val *= 2;
	return val > 180 ? val - 360 : val;
}

int int2span(int val){
	if (val < 0 || 360 < val) Error(std::format("Value {} is out of range [0 - 360]", val));
	return val / 2;
}

int span2int(int val){
	return val * 2;
}

std::string GetCoordinateText(void){
	if(m_cartesian_pos.get()){
		return std::format(
			"{}x{},\n{},{}",
			dist2int(m_height.get()),
			dist2int(m_width.get()),
			dist2int(m_x.get()),
			dist2int(m_y.get())
		);
	}

	return std::format(
		"{}m\n{},{}",
		dist2int(m_dist.get()),
		angle2int(m_dir.get()),
		span2int(m_span.get())
	);
}

void GetCoordinateBin(CChipBinary& bin){
	if(m_cartesian_pos.get()){
		m_cartesian_pos.GetBin(bin);
		m_height.GetBin(bin);
		m_width.GetBin(bin);
		m_x.GetBin(bin);
		m_y.GetBin(bin);
	}else{
		m_dummy.GetBin(bin);
		m_span.GetBin(bin);
		m_dist.GetBin(bin);
		m_dir.GetBin(bin);
	}
}

void SetCoordinateBin(CChipBinary& bin){
	// 未実装
}

// option
auto& h(int param)	{m_height	= int2dist(param);  m_cartesian_pos = 1;				return *this;}
auto& w(int param)	{m_width	= int2dist(param);  m_cartesian_pos = 1;				return *this;}
auto& x(int param)	{m_x		= int2dist(param);  m_cartesian_pos = 1;				return *this;}
auto& y(int param)	{m_y		= int2dist(param);  m_cartesian_pos = 1;				return *this;}
auto& span(int param){m_span	= int2span(param);  m_cartesian_pos = 0; m_height = 0;	return *this;}
auto& dist(int param){m_dist	= int2dist(param);  m_cartesian_pos = 0; m_height = 0;	return *this;}
auto& dir(int param)	{m_dir	= int2angle(param); m_cartesian_pos = 0; m_height = 0;	return *this;}

ScaledInt<1>	m_cartesian_pos;
ScaledInt<7>	m_height;
ScaledInt<8>	m_width;
ScaledInt<8>	m_x;
ScaledInt<8>	m_y;

ScaledInt<8>	m_dummy;
ScaledInt<8>	m_span;
ScaledInt<8>	m_dist;
ScaledInt<8>	m_dir;
