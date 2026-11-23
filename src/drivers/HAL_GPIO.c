#include "HAL_GPIO.h"
#include <stdint.h>



uint32_t PINPOS[16] = {
	(0x00),
	(0x04),
	(0x08),
	(0x0C),
	(0x10),
	(0x14),
	(0x18),
	(0x1C),
	(0x00),
	(0x04),
	(0x08),
	(0x0C),
	(0x10),
	(0x14),
	(0x18),
	(0x1C),
};


void config_pin (GPIO_TypeDef *port, uint32_t pinNumber, uint32_t mode_type)
{
	
	if (pinNumber >= 8)
	{
		switch(mode_type)
		{
			case OUTPUT_GEN_PURPOSE | INPUT_ANALOG:
				port->CRH &= ~((1<<CNF_POS_BIT1) | (1<<CNF_POS_BIT2));
			break;

			case OUTPUT_OD | INPUT_FLOATING:
				port->CRH &= ~((1<<CNF_POS_BIT2));
				port->CRH |= (1<<CNF_POS_BIT1);
			break;

			case OUTPUT_ALT_FUNCTION | INPUT_PU_DP:
				port->CRH |= (OUTPUT_ALT_FUNCTION << CNF_POS_BIT1);
			break;

			case OUTPUT_ALT_FUNCTION_OD:
				port->CRH |= (OUTPUT_ALT_FUNCTION_OD << CNF_POS_BIT1);
			break;

		} // end switch 
	}
	else
	{
		switch(mode_type)
		{
			case OUTPUT_GEN_PURPOSE | INPUT_ANALOG:
				port->CRL &= ~((1<<CNF_POS_BIT1) | (1<<CNF_POS_BIT2));
			break;

			case OUTPUT_OD | INPUT_FLOATING:
				port->CRL &= ~((1<<CNF_POS_BIT2));
				port->CRL |= (1<<CNF_POS_BIT1);
			break;

			case OUTPUT_ALT_FUNCTION | INPUT_PU_DP:
				port->CRL |= (OUTPUT_ALT_FUNCTION << CNF_POS_BIT1);
			break;

			case OUTPUT_ALT_FUNCTION_OD:
				port->CRL |= (OUTPUT_ALT_FUNCTION_OD << CNF_POS_BIT1);
			break;

		} // end switch
	}
}

void config_pin_speed (GPIO_TypeDef *port, uint32_t pinNumber, uint32_t pinSpeed, uint32_t mode)
{
	if(pinNumber >= 8)
	{
		if(mode == INPUT_MODE) 
		{
			port->CRH &= ~( (1<<(PINPOS[pinNumber])) | (1<<(PINPOS[pinNumber] + 1)) );
		}
		else
		{
			port->CRH |= (pinSpeed << (PINPOS[pinNumber]));
		}
	}
	else
	{
		if(mode == INPUT_MODE) 
		{
			port->CRL &= ~( (1<<(PINPOS[pinNumber])) | (1<<(PINPOS[pinNumber] + 1)) );
		}
		else
		{
			port->CRL |= (pinSpeed << (PINPOS[pinNumber]));
		}
	}
}

//===============================================================================

void gpio_write(GPIO_TypeDef *port, uint32_t pinNumber, uint8_t state)
{
	if(state)
	{
		port->BSRR = (1<<pinNumber);		//	HIGH
	}
	else
	{
		port->BSRR = (1<<(pinNumber + 16));	//	LOW
	}
}
/*
*переключение пина (PORT *, PIN);
*/
void gpio_toggle(GPIO_TypeDef *port, uint32_t pinNumber)
{
	port->ODR ^= (1<<pinNumber);
}

void gpio_init(GPIO_TYPE gpio_type)
{
	if(gpio_type.port == PORTA) GPIO_CLOCK_ENABLE_PORTA;
	if(gpio_type.port == PORTB) GPIO_CLOCK_ENABLE_PORTB;
	if(gpio_type.port == PORTC) GPIO_CLOCK_ENABLE_PORTC;
	if(gpio_type.port == PORTD) GPIO_CLOCK_ENABLE_PORTD;
	
	config_pin (gpio_type.port, gpio_type.pin, gpio_type.mode_type);
	config_pin_speed (gpio_type.port, gpio_type.pin, gpio_type.speed, gpio_type.mode);
}

//===============================================================================
//===============================================================================
//===============================================================================
//


void configure_gpio_interrupt (GPIO_TypeDef *port, uint32_t pinNumber, edge_select edge)
{
	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;

	if(port == PORTA)
	{
		switch(pinNumber)
		{
			case 0:
			AFIO->EXTICR[0] = AFIO_EXTICR1_EXTI0_PA;
		break;
		case 1:
			AFIO->EXTICR[0] = AFIO_EXTICR1_EXTI1_PA;
		break;
		case 2:
			AFIO->EXTICR[0] = AFIO_EXTICR1_EXTI2_PA;
		break;
		case 3:
			AFIO->EXTICR[0] = AFIO_EXTICR1_EXTI3_PA;
		break;
		case 4:
			AFIO->EXTICR[1] = AFIO_EXTICR2_EXTI4_PA;
		break;
		case 5:
			AFIO->EXTICR[1] = AFIO_EXTICR2_EXTI5_PA;
		break;
		case 6:
			AFIO->EXTICR[1] = AFIO_EXTICR2_EXTI6_PA;
		break;
		case 7:
			AFIO->EXTICR[1] = AFIO_EXTICR2_EXTI7_PA;
		break;
		case 8:
			AFIO->EXTICR[2] = AFIO_EXTICR3_EXTI8_PA;
		break;
		case 9:
			AFIO->EXTICR[2] = AFIO_EXTICR3_EXTI9_PA;
		break;
		case 10:
			AFIO->EXTICR[2] = AFIO_EXTICR3_EXTI10_PA;
		break;
		case 11:
			AFIO->EXTICR[2] = AFIO_EXTICR3_EXTI11_PA;
		break;
		case 12:
			AFIO->EXTICR[3] = AFIO_EXTICR4_EXTI12_PA;
		break;
		case 13:
			AFIO->EXTICR[3] = AFIO_EXTICR4_EXTI13_PA;
		break;
		case 14:
			AFIO->EXTICR[3] = AFIO_EXTICR4_EXTI14_PA;
		break;
		case 15:
			AFIO->EXTICR[3] = AFIO_EXTICR4_EXTI15_PA;
		break;
		}// END switch
	}
	if(port == PORTB)
	{
		switch(pinNumber)
		{
			case 0:
			AFIO->EXTICR[0] = AFIO_EXTICR1_EXTI0_PB;
		break;
		case 1:
			AFIO->EXTICR[0] = AFIO_EXTICR1_EXTI1_PB;
		break;
		case 2:
			AFIO->EXTICR[0] = AFIO_EXTICR1_EXTI2_PB;
		break;
		case 3:
			AFIO->EXTICR[0] = AFIO_EXTICR1_EXTI3_PB;
		break;
		case 4:
			AFIO->EXTICR[1] = AFIO_EXTICR2_EXTI4_PB;
		break;
		case 5:
			AFIO->EXTICR[1] = AFIO_EXTICR2_EXTI5_PB;
		break;
		case 6:
			AFIO->EXTICR[1] = AFIO_EXTICR2_EXTI6_PB;
		break;
		case 7:
			AFIO->EXTICR[1] = AFIO_EXTICR2_EXTI7_PB;
		break;
		case 8:
			AFIO->EXTICR[2] = AFIO_EXTICR3_EXTI8_PB;
		break;
		case 9:
			AFIO->EXTICR[2] = AFIO_EXTICR3_EXTI9_PB;
		break;
		case 10:
			AFIO->EXTICR[2] = AFIO_EXTICR3_EXTI10_PB;
		break;
		case 11:
			AFIO->EXTICR[2] = AFIO_EXTICR3_EXTI11_PB;
		break;
		case 12:
			AFIO->EXTICR[3] = AFIO_EXTICR4_EXTI12_PB;
		break;
		case 13:
			AFIO->EXTICR[3] = AFIO_EXTICR4_EXTI13_PB;
		break;
		case 14:
			AFIO->EXTICR[3] = AFIO_EXTICR4_EXTI14_PB;
		break;
		case 15:
			AFIO->EXTICR[3] = AFIO_EXTICR4_EXTI15_PB;
		break;
		}// END switch
	}
	if(port == PORTC)
	{
		switch(pinNumber)
		{
			case 0:
			AFIO->EXTICR[0] = AFIO_EXTICR1_EXTI0_PC;
		break;
		case 1:
			AFIO->EXTICR[0] = AFIO_EXTICR1_EXTI1_PC;
		break;
		case 2:
			AFIO->EXTICR[0] = AFIO_EXTICR1_EXTI2_PC;
		break;
		case 3:
			AFIO->EXTICR[0] = AFIO_EXTICR1_EXTI3_PC;
		break;
		case 4:
			AFIO->EXTICR[1] = AFIO_EXTICR2_EXTI4_PC;
		break;
		case 5:
			AFIO->EXTICR[1] = AFIO_EXTICR2_EXTI5_PC;
		break;
		case 6:
			AFIO->EXTICR[1] = AFIO_EXTICR2_EXTI6_PC;
		break;
		case 7:
			AFIO->EXTICR[1] = AFIO_EXTICR2_EXTI7_PC;
		break;
		case 8:
			AFIO->EXTICR[2] = AFIO_EXTICR3_EXTI8_PC;
		break;
		case 9:
			AFIO->EXTICR[2] = AFIO_EXTICR3_EXTI9_PC;
		break;
		case 10:
			AFIO->EXTICR[2] = AFIO_EXTICR3_EXTI10_PC;
		break;
		case 11:
			AFIO->EXTICR[2] = AFIO_EXTICR3_EXTI11_PC;
		break;
		case 12:
			AFIO->EXTICR[3] = AFIO_EXTICR4_EXTI12_PC;
		break;
		case 13:
			AFIO->EXTICR[3] = AFIO_EXTICR4_EXTI13_PC;
		break;
		case 14:
			AFIO->EXTICR[3] = AFIO_EXTICR4_EXTI14_PC;
		break;
		case 15:
			AFIO->EXTICR[3] = AFIO_EXTICR4_EXTI15_PC;
		break;
		}// END switch
	}
	if(port == PORTD)
	{
		switch(pinNumber)
		{
			case 0:
			AFIO->EXTICR[0] = AFIO_EXTICR1_EXTI0_PD;
		break;
		case 1:
			AFIO->EXTICR[0] = AFIO_EXTICR1_EXTI1_PD;
		break;
		case 2:
			AFIO->EXTICR[0] = AFIO_EXTICR1_EXTI2_PD;
		break;
		case 3:
			AFIO->EXTICR[0] = AFIO_EXTICR1_EXTI3_PD;
		break;
		case 4:
			AFIO->EXTICR[1] = AFIO_EXTICR2_EXTI4_PD;
		break;
		case 5:
			AFIO->EXTICR[1] = AFIO_EXTICR2_EXTI5_PD;
		break;
		case 6:
			AFIO->EXTICR[1] = AFIO_EXTICR2_EXTI6_PD;
		break;
		case 7:
			AFIO->EXTICR[1] = AFIO_EXTICR2_EXTI7_PD;
		break;
		case 8:
			AFIO->EXTICR[2] = AFIO_EXTICR3_EXTI8_PD;
		break;
		case 9:
			AFIO->EXTICR[2] = AFIO_EXTICR3_EXTI9_PD;
		break;
		case 10:
			AFIO->EXTICR[2] = AFIO_EXTICR3_EXTI10_PD;
		break;
		case 11:
			AFIO->EXTICR[2] = AFIO_EXTICR3_EXTI11_PD;
		break;
		case 12:
			AFIO->EXTICR[3] = AFIO_EXTICR4_EXTI12_PD;
		break;
		case 13:
			AFIO->EXTICR[3] = AFIO_EXTICR4_EXTI13_PD;
		break;
		case 14:
			AFIO->EXTICR[3] = AFIO_EXTICR4_EXTI14_PD;
		break;
		case 15:
			AFIO->EXTICR[3] = AFIO_EXTICR4_EXTI15_PD;
		break;
		}// END switch
	}

	if(edge == RISING_EDGE) EXTI->RTSR |= 1<<pinNumber;
	if(edge == FALLING_EDGE) EXTI->FTSR |= 1<<pinNumber;
	if(edge == RISING_FALLING_EDGE) 
	{
		EXTI->RTSR |= 1<<pinNumber;
		EXTI->FTSR |= 1<<pinNumber;
	}
		

}


//======================================================================
//

void enable_gpio_interrupt (uint32_t pinNumber, IRQn_Type irqNumber)
{
	// enable interrupt in EXTI
	EXTI->IMR |= 1<<pinNumber;
	// enable interrupt in NVIC
	NVIC_EnableIRQ(irqNumber);
}

//======================================================================
//

void clear_gpio_interrupt (uint32_t pinNumber)
{
	EXTI->PR |= 1<<pinNumber;
}
