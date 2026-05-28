#ifndef dMUX_H
#define dMUX_H

class dMUX
{
public:
	dMUX();

	// Public API
    int Get_Receiver_Configuration_State();
    bool Set_Receiver_Configuration_State(int configuration_number);

private:
    void Configure_For_VHF_Charles();
    void Configure_For_VHF_External();
    void Configure_For_HF();

	enum receiver_configuration
    {
        HF,
        VHF_CHARLES,
        VHF_EXTERNAL
    } state = HF;
	
    dMUX(const dMUX&);
    void operator=(const dMUX&);
};

extern dMUX g_MUX;

#endif  // dMUX_H